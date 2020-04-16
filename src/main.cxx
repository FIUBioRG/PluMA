/******************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018 Bioinformatics Research Group (BioRG)
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

\******************************************************************************/

#include <ctime>
#include <cstring>
#include <dlfcn.h>
#include <error.h>
#include <fstream>
#include <argp.h>
#include <glob.h>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>
#include "Plugin.h"
//#include "PluginManager.h"
#include "PluginProxy.h"
#include "languages/Compiled.h"
#include "languages/Perl.h"
#include "languages/Py.h"
#include "languages/R.h"

#undef VERSION
#define VERSION "1.1.0"

#ifdef WITH_HEALTHCHECK
#define WITH_HEALTHCHECK
#include <thread>
#include "healthcheck.h"
#define DEFAULT_PORT 3000
#endif

const char *argp_program_version = VERSION;
const char *argp_program_bug_address = "<tcickovs@fiu.edu>";
char args_doc[] = "[CONFIG FILE] [<optional restart point>]";
static struct argp_option options[] = {
    {"log-file", 'l', "FILE", 0, "File to output logging information to. [default: ./logs/[TIMESTAMP].log.txt"},
    {"error-file", 'e', "FILE", 0, "File to output error information to. [default: /dev/stderr]"},
    {"healthcheck", 'H', 0, 0, "Enable the healthcheck socket. PluMA must be compiled with -DWITH_HEALTHCHECK. [default: false]"},
    {0}
};

typedef struct {
    char *args[2];
    char *log_file, error_file;
} arguments;

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    arguments *arguments = reinterpret_cast<::arguments *>(state->input);

    switch(key) {
        case ARGP_KEY_ARG:
            if(state->arg_num >= 2) {
                argp_usage(state);
            }
            arguments->args[state->arg_num] = arg;
            break;
        case ARGP_KEY_NO_ARGS:
            argp_usage(state);
            break;
        case ARGP_KEY_END:
            if(state->arg_num < 1) {
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, 0 /* TODO: potential banner */ };

////////////////////////////////////////////////////////////////////////////////
// Helper Function: Convert int to string
std::string toString(int val) {
    std::string retval;
    std::stringstream ss;
    ss << val;
    ss >> retval;
    return retval;
}
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    ////////////////////////////////////////////////////////////////////////////
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
    std::cout << "*               (C) 2016, 2018-2020 Bioinformatics Research Group,                *" << std::endl;
    std::cout << "*                         Florida International University                        *" << std::endl;
    std::cout << "*    Under MIT License From Open Source Initiative (OSI), All Rights Reserved.    *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*    Any professionally published work using PluMA should cite the following:     *" << std::endl;
    std::cout << "*                        T. Cickovski and G. Narasimhan.                          *" << std::endl;
    std::cout << "*                Constructing Lightweight and Flexible Pipelines                  *" << std::endl;
    std::cout << "*                 Using Plugin-Based Microbiome Analysis (PluMA)                  *" << std::endl;
    std::cout << "*                     Bioinformatics 34(17):2881-2888, 2018                       *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "***********************************************************************************" << std::endl;

    ////////////////////////////////////////////////////////////////////////////
    // Setup plugin path.
    std::string pluginpath = "/plugins/";
    pluginpath = getenv("PWD") + pluginpath;
    if  (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        pluginpath += ":";
        pluginpath += getenv("PLUMA_PLUGIN_PATH");
    }
    pluginpath += ":";
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Set supported languages here.
    std::vector<Language*> supported;
    std::map<std::string, std::string> pluginLanguages;
    supported.push_back(new Compiled("C", "so", pluginpath, "lib"));
    supported.push_back(new Py("Python", "py", pluginpath));
    supported.push_back(new MiAMi::R("R", "R", pluginpath, argc, argv));
    supported.push_back(new Perl("Perl", "pl", pluginpath));
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Command line arguments
    arguments arguments;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // For each supported language, load the appropriate plugins
    std::string path = pluginpath.substr(0, pluginpath.find_first_of(":"));
    bool list = false;
    while (path.length() > 0) {
        glob_t globbuf;

        if (std::string(argv[1]) == "plugins") {
            std::cout << "[PluMA] Current plugin list: " << std::endl;
            list = true;
        }

        for (int i = 0; i < supported.size(); i++) {
            supported[i]->loadPlugin(path, &globbuf, &pluginLanguages, list);
        }

        std::map<std::string, std::string>::iterator itr;

        for (itr = pluginLanguages.begin();
            itr != pluginLanguages.end(); itr++)
        {
            PluginManager::getInstance().add(
                itr->first.substr(0, itr->first.length() - 6)
            );
        }

        globfree(&globbuf);

        pluginpath = pluginpath.substr(pluginpath.find_first_of(":") + 1,
            pluginpath.length());
        path = pluginpath.substr(0, pluginpath.find_first_of(":"));
    }

    if (list) exit(EXIT_SUCCESS);
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Check for a restart point
    bool doRestart = false;
    std::string restartPoint = "";
    if (argc == 3) {
        doRestart = true;
        restartPoint = std::string(argv[2]);
    }
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // Get the current time, and setup the initial log file
    time_t t = time(0);
    struct tm *now = localtime(&t);
    std::string currentTime = toString(now->tm_year + 1900) + "-"
        + toString(now->tm_mon + 1) + "-" + toString(now->tm_mday) + "@"
        + toString(now->tm_hour) + ":" + toString(now->tm_min) + ":"
        + toString(now->tm_sec);
    std::string mylog = "logs/"+ currentTime + ".log.txt";
    PluginManager::getInstance().setLogFile(mylog);

#ifdef WITH_HEALTHCHECK
    ////////////////////////////////////////////////////////////////////////////
    // Prepare thread for running the heartbeat socket.
    std::thread socket_thread(heartbeat, DEFAULT_PORT);
#endif

    ////////////////////////////////////////////////////////////////////////////
    // Read configuration file and make appropriate plugins
    std::ifstream infile(argv[1], std::ios::in);
    bool restartFlag = false;
    std::string prefix = "";
    while (!infile.eof()) {
        ////////////////////////////////////////////////////////////////////////
        // Read one line
        // Plugin (Name) inputfile (input file) outputfile (output file)
        // TODO: suggest moving to getopt or other commandline parser
        std::string junk, name, inputname, outputname;
        infile >> junk;
        // comment
        if (junk[0] == '#') {
            getline(infile, junk);
            continue;
        } else if (junk == "Prefix") { // prefix
            infile >> prefix;
            prefix += "/";
            continue;
        } else {
            infile >> name >> junk >> inputname >> junk >> outputname;
        }
        if (inputname[0] != '/')
            inputname = prefix + inputname;
        if (outputname[0] != '/')
            outputname = prefix + outputname;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // If we are restarting and have not hit that point yet,
        // skip this plugin
        if (doRestart && !restartFlag) {
            if (name == restartPoint) {
                restartFlag = true;
            } else {
                continue;
            }
        }
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // Try to create and run all three steps of the plugin in the
        // appropriate language
        PluginManager::getInstance().log("Creating plugin "+name);
        try {
            bool executed = false;
            for (int i = 0; i < supported.size() && !executed; i++)
                if (pluginLanguages[name+"Plugin"] == supported[i]->lang()) {
                    std::cout << "[PluMA] Running Plugin: " << name << std::endl;
                    supported[i]->executePlugin(name, inputname, outputname);
                    executed = true;
                }
            ////////////////////////////////////////////////////////////////////
            // In this case we found the plugin, but the language
            // is not supported.
            if (!executed && name != "")
                PluginManager::getInstance().log("Error, no suitable language for plugin: " + name + ".");
            ////////////////////////////////////////////////////////////////////
#ifdef WITH_HEALTHCHECK
#endif
        }
        ////////////////////////////////////////////////////////////////////////
        // This hits if the plugin errored while running it
        // Message(s) will be output to the logfile, and
        // output files will be removed.
        catch (...) {
            PluginManager::getInstance().log("ERROR IN PLUGIN: "+name+".");;
            if (access( outputname.c_str(), F_OK ) != -1 ) {
                PluginManager::getInstance().log("REMOVING OUTPUT FILE: "
                    + outputname + ".");
                int result = system(("rm "+outputname).c_str());
                exit(EXIT_FAILURE);
            }
        }
        ////////////////////////////////////////////////////////////////////////
    }
    ////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////
    // Cleanup.
#ifdef WITH_HEALTHCHECK
    // Wait for any socket use to be completed.
    socket_thread.join();
#endif
    for (int i = 0; i < supported.size(); i++)
        supported[i]->unload();
    ////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////
    // El Finito!
    return 0;
    ////////////////////////////////////////////////////////////////////////////
}
