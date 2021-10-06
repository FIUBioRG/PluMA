/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018, 2019-2021 Bioinformatics Research Group (BioRG)
                       Florida International University


     Permission is hereby granted, free of charge, to any person obtaining
          a copy of this software and associated documentation files
        (the "Software"), to deal in the Software without restriction,
      including without limitation the rights to use, copy, modify, merge,
      publish, distribute, sublicense, and/or sell copies of the Software,
       and to permit persons to whom the Software is furnished to do so,
                    subject to the following conditions:

    The above copyright notice and this permission notice shall be included
            in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
           SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\*********************************************************************************/

#include <argp.h>
#include <dlfcn.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "execbuf.hxx"
#include "Plugin.h"
#include "PluginProxy.h"

/**
 * Actions the PluMA application can take.
 *
 * @since v2.1.0
 */
enum PLUMA_MAIN_ACTIONS {
    ACTION_PIPELINE,
    ACTION_PRINT_PLUGINS,
    ACTION_INSTALL_DEPENDENCIES,
    ACTION_COMPILE_PLUGINS
};

enum PLUMA_ERROR {
    SUCCESS = 00U,
    ERR_INSTALL_PYTHON_DEP = 01U,
    ERR_INSTALL_LINUX_DEP = 02U,
    ERR_INSTALL_MACOS_DEP = 03U,
    ERR_INSTALL_WINDOWS_DEP = 04U
};

std::vector<std::string> split(std::string str, const std::string delim) {
    std::vector<std::string> tokens;
    size_t pos = 0;

    while((pos = str.find(delim)) != std::string::npos) {
        tokens.push_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }

    return tokens;
}

// Auto-generated program version
const char* argp_program_version = "PluMA v2.1.0";
// Bug address for submitting issues. Github URL instead of email address.
const char* argp_program_bug_address = "<https://github.com/FIUBioRG/PluMA/issues>";
// Top-level doc block
static char doc[] = "PluMA - Plugin-Based Microbiome Analysis";
// Arguments documentation
static char args_doc[] = "[CONFIGURATION FILE] [OPTIONAL: RESTART POINT]";

/**
 * Commandline options
 *
 * @since v2.1.0
 */
static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"install-dependencies", 'i', 0, 0, "Install dependencies of all plugins in the PLUMA_PLUGINS_PATH (default: '$(pwd)/plugins')"},
    {"compile-plugins", 'c', 0, 0, "Compile C++/CUDA plugins found in PLUMA_PLUGINS_PATH"},
    {"plugins", 'p', 0, 0, "Prints a list of currently installed plugins in PLUMA_PLUGINS_PATH"},
    { 0 }
};

/**
 * Commandline arguments structure
 *
 * @since v2.1.0
 */
struct arguments {
    int verbose;
    char* config_file;
    char* restart_point;
    bool do_restart;
    PLUMA_MAIN_ACTIONS action;
};

/**
 * Parse single options.
 *
 * @since v2.1.0
 *
 * @param key Key that is being parsed.
 * @param arg Arguments character pointer.
 * @param state State of the argp processing.
 * @return An error if there is any form of error in parsing our options.
 */
static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arguments* arguments = (struct arguments*) state->input;

    switch(key) {
        case 'v':
            arguments->verbose = 1;
            break;

        case 'i':
            arguments->action = PLUMA_MAIN_ACTIONS::ACTION_INSTALL_DEPENDENCIES;
            break;

        case 'p':
            arguments->action = PLUMA_MAIN_ACTIONS::ACTION_PRINT_PLUGINS;
            break;

        case 'c':
            arguments->action = PLUMA_MAIN_ACTIONS::ACTION_COMPILE_PLUGINS;
            break;

        case ARGP_KEY_ARG:
            if(state->arg_num >= 2) {
                // Too many arguments
                std::cerr << "Too many arguments specified." << std::endl;
                argp_usage(state);
            }
            if (state->arg_num == 0) {
                arguments->config_file = arg;
            }
            if (state->arg_num == 1) {
                arguments->do_restart = true;
                arguments->restart_point = arg;
            }
            break;

        case ARGP_KEY_END:
            if(state->arg_num < 1 && arguments->action == ACTION_PIPELINE) {
                // Not enough arguments
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

// Parse our arguments
static struct argp argp = { options, parse_opt, args_doc, doc };

/**
 * Read a PluMA configuration file for use in running a
 * Bioinformatics pipeline.
 *
 * @since v1.0.0
 *
 * @param inputfile The file input to be parsed as a configuration.
 * @param prefix The prefix to be given to the input.
 * @param doRestart Whether we are restarting the pipeline from a certain point.
 * @param restartPoint The point at which we restart the pipeline.
 * @param pluginManager The currently used PluginManager instance.
 */
void readConfig(char *inputfile, std::string prefix, bool doRestart, char* restartPoint)
{
    std::ifstream infile(inputfile, std::ios::in);
    bool restartFlag = false;
    std::string pipeline = "";
    std::string oldprefix = prefix;
    std::string kitty = "";

    while (!infile.eof()) {
        std::string junk, name, inputname, outputname;
        /*
        * Read one line
        * Plugin (Name) inputfile (input file) outputfile (output file)
        */
        infile >> junk;

        if (junk[0] == '#') {
            // the line is a comment
            getline(infile, junk);
            continue;
        } else if (junk == "Prefix") {
            // the line is a prefix
            infile >> prefix;
            prefix += "/";
            PluginManager::myPrefix = prefix;
            oldprefix = prefix;
            continue;
        } else if (junk == "Pipeline") {
            infile >> pipeline;
            readConfig((char *)pipeline.c_str(), prefix, false, (char*) "");
        } else if (junk == "Kitty") {
            infile >> kitty;
            if (oldprefix != "")
            {
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
                if (PluginManager::getInstance().pluginLanguages[name+"Plugin"] == PluginManager::supported[i]->lang()) {
                    std::cout << "[PluMA] Running Plugin: " << name << std::endl;
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
            if (access(outputname.c_str(), F_OK) != -1)
            {
                int c;
                PluginManager::getInstance().log("REMOVING OUTPUT FILE: " + outputname + ".");
                c = system(("rm " + outputname).c_str());
                if (c != 0) {
                    std::cerr << "Error while removing output file..." << std::endl;
                    PluginManager::getInstance().log("Error while removing output file");
                }
                exit(1);
            }
        }
    }
}

/**
 * Print the standard PluMA banner
 *
 * @since v2.1.0
 */
void print_banner() {
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Display opening screen.
    std::cout << "***********************************************************************************" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*  PPPPPPPPPPPP    L               U           U   M           M         A        *" << std::endl;
    std::cout << "*  P           P   L               U           U   MM         MM        A A       *" << std::endl;
    std::cout << "*  P           P   L               U           U   M M       M M       A   A      *" << std::endl;
    std::cout << "*  P           P   L               U           U   M  M     M  M      A     A     *" << std::endl;
    std::cout << "*  P           P   L               U           U   M   M   M   M     A       A    *" << std::endl;
    std::cout << "*  PPPPPPPPPPPP    L               U           U   M    M M    M    A         A   *" << std::endl;
    std::cout << "*  P               L               U           U   M     M     M   AAAAAAAAAAAAA  *" << std::endl;
    std::cout << "*  P               L               U           U   M           M   A           A  *" << std::endl;
    std::cout << "*  P               L               U           U   M           M   A           A  *" << std::endl;
    std::cout << "*  P               LLLLLLLLLLLLL    UUUUUUUUUUU    M           M   A           A  *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*            [Plu]gin-based [M]icrobiome [A]nalysis (formerly MiAMi)              *" << std::endl;
    std::cout << "*                          (C) 2016, 2018, 2019-2021                              *" << std::endl;
    std::cout << "*          Bioinformatics Research Group, Florida International University        *" << std::endl;
    std::cout << "*    Under MIT License From Open Source Initiative (OSI), All Rights Reserved.    *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*    Any professionally published work using PluMA should cite the following:     *" << std::endl;
    std::cout << "*                        T. Cickovski and G. Narasimhan.                          *" << std::endl;
    std::cout << "*                Constructing Lightweight and Flexible Pipelines                  *" << std::endl;
    std::cout << "*                 Using Plugin-Based Microbiome Analysis (PluMA)                  *" << std::endl;
    std::cout << "*                     Bioinformatics 34(17):2881-2888, 2018                       *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "***********************************************************************************" << std::endl;
}

/**
 * Install dependencies for a given platform.
 *
 * @since v2.1.0
 * @param platform The platform the dependencies are being installed for (Eg: Python, Linux...).
 */
void install_dependencies(const std::string platform, const std::string command, const std::map<std::string, std::string>* dependencies) {
    std::cout << "Installing " << platform << " dependencies..." << std::endl;

    for (auto it = dependencies->cbegin(); it != dependencies->cend(); it++) {
        std::string answer;
        std::cout << it->first << " has dependencies to install. Install them? (Y/N) ";

        std::cin >> answer;

        if (answer == "y" || answer == "Y") {
            std::string cmd = command + " " + it->second;

            try {
                pluma::execbuf(cmd.c_str());
            } catch (...) {
                std::cerr << "An error occurred while installing plugin dependencies for " << it->first << std::endl;
            }
        }
    }

}

int main(int argc, char** argv) {
    struct arguments arguments;

    // Default argument configuration
    arguments.verbose = 0;
    arguments.action = PLUMA_MAIN_ACTIONS::ACTION_PIPELINE;
    arguments.do_restart = false;
    arguments.restart_point = (char*) "";
    arguments.config_file = (char*) "";

    // Parse commandline arguments using argp library
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Print the PluMA banner if argument parsing is successful
    print_banner();

    // Setup plugin path.
    std::string pluginpath = std::string(getenv("PWD")) + "/plugins/";
    if (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        pluginpath += ":";
        pluginpath += std::string(getenv("PLUMA_PLUGIN_PATH"));
    }
    pluginpath += ":";

    PluginManager::supportedLanguages(pluginpath, argc, argv);

    std::vector<std::string> paths = split(pluginpath, ":");
    std::map<std::string, std::string> python_deps, linux_deps,
        macos_deps, windows_deps;

    switch(arguments.action) {
        // Install dependencies for all plugins
        case PLUMA_MAIN_ACTIONS::ACTION_INSTALL_DEPENDENCIES:
            for(auto const &path: paths) {
                std::cout << "Searching for plugin dependency files in " << path << std::endl;

                for (auto const p: std::filesystem::directory_iterator(path)) {
                    std::string fp = p.path().string();
                    std::string name = fp.substr(fp.find_last_of("/") + 1, fp.length());

                    // Check for Python dependencies.
                    if (std::filesystem::exists(fp + "requirements.txt")) {
                        python_deps.insert({name, fp + "requirements.txt"});
                    }
#if __linux__
                    // Check for Linux (Ubuntu) dependencies.
                    // These dependencies should be `sh` compatible
                    if (std::filesystem::exists(fp + "requirements.sh")) {
                        linux_deps.insert({name, fp + "requirements.sh"});
                    }
#elif __APPLE__
                    /*
                     * Check for macOS (Brew) dependencies.
                     * These dependencies should be Bourne Shell/Z-shell compatible
                     * and should use the Brew package manager for installation
                     * or relevent command-line inputs
                     */
                    if (std::filesystem::exists(fp + "requirements-macos.sh")) {
                        macos_deps.insert({name, fp + "requirements-macos.sh"});
                    }
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
                    /*
                     * Check for Windows batch files to install dependencies.
                     * @TODO: Make work correctly with Windows
                     */
                    if (std::filesystem::exists(fp + "requirements.bat")) {
                        windows_deps.insert({name, fp + "requirements.bat"});
                    }
#else
#error "Unknown operating system"
#endif
                }
            }

            if (!python_deps.empty()) {
                install_dependencies("Python", "pip install -r", &python_deps);
            } else {
                std::cout << "No Python dependencies found..." << std::endl;
            }
#if __linux__
            if (!linux_deps.empty()) {
                install_dependencies("Linux", "sh", &linux_deps);
            } else {
                std::cout << "No Linux dependencies found..." << std::endl;
            }
#elif __APPLE__
            if (!macos_deps.empty()) {
                // @TODO: Implement macOS installation of dependencies
            } else {
                std::cout << "No macOS dependencies found..." << std::endl;
            }
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
            if (!windows_deps.empty()) {
                // @TODO: Implement Windows installation of dependencies
            } else {
                std::cout << "No Windows dependencies found..." << std:endl;
            }
#else
#error "Unknown operating system"
#endif
            return 0;
            break;

        // Compile cloned C++/CUDA plugins
        case PLUMA_MAIN_ACTIONS::ACTION_COMPILE_PLUGINS:
            // @TODO: Work on compilation system
            std::cout << "This feature is not currently completed." << std::endl;
            break;

        // Print a list of installed plugins
        case PLUMA_MAIN_ACTIONS::ACTION_PRINT_PLUGINS:
            for(auto const& path: paths) {
                std::cout << "Currently installed plugins in " << path << ":" << std::endl;

                std::vector<std::string> plugins;

                for (auto const& plugin: std::filesystem::directory_iterator(path)) {
                    if (plugin.is_directory()) {
                        std::string p = plugin.path();
                        plugins.push_back(p.substr(p.find_last_of("/") + 1, p.length()));
                    }
                }

                std::sort(plugins.begin(), plugins.end());

                for (auto const &plugin : plugins) {
                    std::cout << "    " << plugin << std::endl;
                }
            }
            return 0;
            break;

        // Follow normal execution
        case PLUMA_MAIN_ACTIONS::ACTION_PIPELINE:
        default:
            glob_t globbuf;

            for (auto const& path: paths) {
                for (unsigned int i = 0; i < PluginManager::supported.size(); i++) {
                    PluginManager::supported[i]->loadPlugin(path, &globbuf, &(PluginManager::getInstance().pluginLanguages));
                }

                for (auto const& itr : PluginManager::getInstance().pluginLanguages) {
                    PluginManager::getInstance().add(itr.first.substr(0, itr.first.length() - 6));
                }
            }

            time_t t = time(0);
            struct tm *now = localtime(&t);

            std::string currentTime = std::to_string(now->tm_year + 1900) + "-" +
                std::to_string(now->tm_mon + 1) + "-" + std::to_string(now->tm_mday) +
                "@" + std::to_string(now->tm_hour) + ":" + std::to_string(now->tm_min) +
                ":" + std::to_string(now->tm_sec);

            std::string logfile = "logs/" + currentTime + ".log.txt";

            PluginManager::getInstance().setLogFile(logfile);

            readConfig(arguments.config_file, "", arguments.do_restart, arguments.restart_point);

            // Clean up
            for (unsigned int i = 0; i < PluginManager::supported.size(); i++) {
                PluginManager::supported[i]->unload();
            }
            // El Finito!
            return 0;
    }
}
