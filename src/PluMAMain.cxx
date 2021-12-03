#include "PluMAMain.hxx"

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <filesystem>

#include "PluginManager.h"
#include "cpp-subprocess/include/subprocess.hpp"
#include "utils.hxx"

using namespace std;
namespace fs = std::filesystem;

PluMA::PluMA() {
    python_deps = new map<string, fs::path>();
    buildfiles =new map<string, fs::path>();
#if __linux__
    linux_deps = new map<string, fs::path>();
#elif __APPLE__
    macos_deps = map<string, fs::path>();
#elif #elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
    windows_deps = new map<string, fs::path>();
#endif

    pluginpath = string(getenv("PWD")) + "/plugins/";
    if (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        pluginpath += ":" + string(getenv("PLUMA_PLUGIN_PATH"));
    }
    pluginpath += ":";
}

/**
 * Read a PluMA configuration file for use in running
 * a Bioinformatics pipeline.
 *
 * @since v1.0.0
 *
 * @param inputfile The file input to be parsed as a configuration.
 * @param prefix The prefix to be given to the input.
 * @param doRestart Whether we are restarting the pipeline from a certain point.
 * @param restartPoint The point at which we restart the pipeline.
 * @param pluginManager The currently used PluginManager instance.
 */
void PluMA::read_config(char *inputfile, string prefix, bool doRestart, char *restartPoint) {
    ifstream infile(inputfile, ios::in);
    bool restartFlag = false;
    string pipeline = "";
    string oldprefix = prefix;
    string kitty = "";

    while (!infile.eof()) {
        string junk, name, inputname, outputname;

        infile >> junk;

        if (junk[0] == '#') {
            getline(infile, junk);
            continue;
        } else if (junk == "Prefix") {
            infile >> prefix;
            prefix += "/";
            PluginManager::myPrefix = prefix;
            oldprefix = prefix;
            continue;
        } else if (junk == "Pipeline") {
            infile >> pipeline;
            read_config((char*) pipeline.c_str(), prefix, false, (char*) "");
        } else if (junk == "Kitty") {
            infile >> kitty;
            if (oldprefix != "") {
                prefix = oldprefix;
            }
            prefix += "/" + kitty + "/";
            PluginManager::myPrefix = prefix;
            continue;
        } else {
            infile >> name >> junk >> inputname >> junk >> outputname;
        }

        if (inputname[0] != '/') {
            inputname = prefix + inputname;
        }

        if (outputname[0] != '/') {
            outputname = prefix + outputname;
        }

        // If we are restarting and have not hit that point yet, skip this plugin
        if (doRestart && !restartFlag) {
            if (name == restartPoint) {
                restartFlag = true;
            } else {
                continue;
            }
        }

        // Try to create and run all three steps of the plugin in the appropriate language
        PluginManager::getInstance().log("Creating plugin " + name);
        try {
            bool executed = false;
            for (unsigned int i = 0; i < PluginManager::supported.size() && !executed; i++) {
                if (PluginManager::getInstance().pluginLanguages[name + "Plugin"] == PluginManager::supported[i]->lang()) {
                    cout << "[PluMA] Running Plugin: " << name << endl;
                    PluginManager::supported[i]->executePlugin(name, inputname, outputname);
                    executed = true;
                }
            }
            // In this case we found the plugin, but the language is not PluginManager::supported.
            if (!executed && name != "") {
                PluginManager::getInstance().log("Error, no suitable language for plugin: " + name + ".");
            }
        } catch (...) {
            /*
             * This hits if the plugin errored while running it
             * Message(s) will be output to the logfile, and output files will be removed.
             */
            PluginManager::getInstance().log("ERROR IN PLUGIN: " + name + ".");
            ;
            if (access(outputname.c_str(), F_OK) != -1) {
                int c;
                PluginManager::getInstance().log("REMOVING OUTPUT FILE: " + outputname + ".");
                c = system(("rm " + outputname).c_str());
                if (c != 0) {
                    std::cerr << "Error while removing output file..." << std::endl;
                    PluginManager::getInstance().log("Error while removing output file");
                }
                exit(EXIT_FAILURE);
            }
        }
    }
}

void PluMA::search() {
    vector<string> paths = utils::split(pluginpath, ":");

    for (auto const &path : paths) {
        std::cout << "Searching for plugin dependency files in " << path << std::endl;

        for (auto p : fs::directory_iterator(path)) {

            string fp = p.path().string();
            string name = fp.substr(fp.find_last_of("/") + 1, fp.length());

            fs::path requirements = fs::path(fp + fs::path::preferred_separator + "requirements.txt");

            if (fs::exists(requirements)) {
                python_deps->insert({name, requirements});
            }

            fs::path sconstruct = fs::path(fp + fs::path::preferred_separator + "SConstruct");

            if (fs::exists(sconstruct)) {
                buildfiles->insert({name, sconstruct});
            }
#if __linux__
            /*
             * Check for Linux (Ubuntu) dependencies.
             * These dependencies should be `sh` compatable`
             */
            fs::path requirements_sh = fs::path(fp + fs::path::preferred_separator + "requirements.sh");

            if (fs::exists(requirements_sh)) {
                linux_deps->insert({name, requirements_sh});
            }
#elif __APPLE__
            /*
             * Check for macOS (Brew) dependencies.
             * These dependencies should be Bourne Shell/Z-shell compatible
             * and should use the Brew package manager for installation
             * or relevent command-line inputs
             */
            if (fs::exists(fp + "requirements-macos.sh")) {
                macos_deps->insert({name, fp + "requirements-macos.sh"});
            }
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
            /*
             * Check for Windows batch files to install dependencies.
             * @TODO: Make work correctly with Windows
             */
            if (fs::exists(fp + "requirements.bat")) {
                windows_deps->insert({name, fp + "requirements.bat"});
            }
#endif
        }
    }
}

/**
 * Install libraries and dependencies for a given platform.
 *
 * @since v2.1.0
 */
void PluMA::install() {
    if (!python_deps->empty()) {
        vector<string> args = { "install", "--no-input", "-r" };
        _install("Python", "Dependencies", "pip", &args, python_deps);
    }
#if __linux__
    if (!linux_deps->empty()) {
        vector<string> args = {};
        _install("Linux", "Dependencies", "sh", &args, linux_deps);
    }
#elif __APPLE__
    if (!macos_deps->empty()) {
        vector<string> args = {};
        _install("MacOS", "Dependencies", "zsh"< &args, macos_deps);
    }
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
    // @TODO:
#endif

    if (!buildfiles->empty()) {
        vector<string> args = {"-f"};
        _install("Library", "Buildfiles", "scons", &args, buildfiles);
    }
}
