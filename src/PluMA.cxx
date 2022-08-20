/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018-2020 Bioinformatics Research Group (BioRG)
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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#include "PluMA.hxx"

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <filesystem>

#include "PluginManager.h"
#include "utils.hxx"

using namespace std;
namespace fs = std::filesystem;

PluMA::PluMA() {
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
            read_config((char *)pipeline.c_str(), prefix, false, (char *)"");
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
            if (fs::exists(outputname)) {
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
