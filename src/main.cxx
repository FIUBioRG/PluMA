/********************************************************************************\

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

\*********************************************************************************/

#include <glob.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "Plugin.h"
//#include "PluginManager.h"
#include "PluginProxy.h"
/*#include "languages/Compiled.h"
#include "languages/Py.h"
#include "languages/R.h"
#include "languages/Perl.h"*/
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>

#include "../extern/cxxopts/include/cxxopts.hpp"

#ifdef WITH_AWS_SDK
#include "../aws-sdk-cpp/src/aws-cpp-sdk-core/source/Aws.cpp"
#endif


//////////////////////////////////////////
// Helper Function: Convert int to string
std::string toString(int val) {
   std::string retval;
   std::stringstream ss;
   ss << val;
   ss >> retval;
   return retval;
}
//////////////////////////////////////////

//////////////////////////////////////////
// Helper Function: Check if String is an Int
bool is_number(const std::string& s) {
    return !s.empty() && std::find_if(
        s.begin(),
        s.end(),
        [](unsigned char c) { return ! std::isdigit(c); }
    ) == s.end();
}
//////////////////////////////////////////

void readConfig(char* inputfile, std::string prefix, bool doRestart, std::string restartPoint) {
    std::ifstream infile(inputfile, std::ios::in);
    bool restartFlag = false;
    std::string pipeline = "";
    std::string oldprefix = prefix;
    std::string kitty = "";
    while (!infile.eof()) {
        std::string junk, name, inputname, outputname;
        /**
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
            PluginManager::myPrefix=prefix;
            oldprefix = prefix;
            continue;
        } else if (junk == "Pipeline") {
            infile >> pipeline;
            readConfig((char*) pipeline.c_str(), prefix, false, "");
        } else if (junk == "Kitty") {
            infile >> kitty;
            if (oldprefix != "") {
                prefix = oldprefix;
            }
            prefix += "/"+kitty+"/";
            PluginManager::myPrefix=prefix;
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
        //////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////
        // If we are restarting and have not hit that point yet, skip this plugin
        if (doRestart && !restartFlag) {
            if (name == restartPoint) {
                restartFlag = true;
            } else {
                continue;
            }
        }
        /////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Try to create and run all three steps of the plugin in the appropriate language
        PluginManager::getInstance().log("Creating plugin "+name);
        try {
            bool executed = false;
            for (int i = 0; i < PluginManager::supported.size() && !executed; i++) {
                if (PluginManager::getInstance().pluginLanguages[name+"Plugin"] == PluginManager::supported[i]->lang()) {
                    std::cout << "[PluMA] Running Plugin: " << name << std::endl;
                    PluginManager::supported[i]->executePlugin(name, inputname, outputname);
                    executed = true;
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
            // In this case we found the plugin, but the language is not PluginManager::supported.
            if (!executed && name != "") {
                PluginManager::getInstance().log("Error, no suitable language for plugin: "+name+".");
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // This hits if the plugin errored while running it
        // Message(s) will be output to the logfile, and output files will be removed.
        catch (...) {
            PluginManager::getInstance().log("ERROR IN PLUGIN: "+name+".");;
            if (access( outputname.c_str(), F_OK ) != -1 ) {
                PluginManager::getInstance().log("REMOVING OUTPUT FILE: "+outputname+".");
                system(("rm "+outputname).c_str());
                exit(1);
            }
        }
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
    }
}


int main(int argc, char** argv)
{
    cxxopts::Options options("pluma", "[Plu]gin-based [M]icrobiome [A]nalysis");

    options.add_options()
        ("a,aws", "We're running on AWS.")
        ("b,build-id", "Containerized Build Id for web-based run.", cxxopts::value<std::string>())
        ("h,help", "Print usage.")
        ("v,version", "Print version.")
        ("p,plugins", "Print installed plugins.");

    options.parse_positional({"input_file", "optional_restart"});

    cxxopts::ParseResult result;

    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::parsing &ex) {
        std::cerr << "pluma: " << ex.what() << std::endl;
        std::cerr << "usage: pluma [options] <input_file> <optional_restart>" << std::endl;
        return EXIT_FAILURE;
    }

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
    std::cout << "* (C) 2016, 2018 Bioinformatics Research Group, Florida International University  *" << std::endl;
    std::cout << "*    Under MIT License From Open Source Initiative (OSI), All Rights Reserved.    *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*    Any professionally published work using PluMA should cite the following:     *" << std::endl;
    std::cout << "*                        T. Cickovski and G. Narasimhan.                          *" << std::endl;
    std::cout << "*                Constructing Lightweight and Flexible Pipelines                  *" << std::endl;
    std::cout << "*                 Using Plugin-Based Microbiome Analysis (PluMA)                  *" << std::endl;
    std::cout << "*                     Bioinformatics 34(17):2881-2888, 2018                       *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "***********************************************************************************" << std::endl;
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Setup plugin path.
    std::string pluginpath = "/plugins/";
    pluginpath = getenv("PWD") + pluginpath;
    if (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        pluginpath += ":";
        pluginpath += getenv("PLUMA_PLUGIN_PATH");
    }
    pluginpath += ":";
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Command line arguments
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (result.count("version")) {
        std::cout << "[PluMA] Version 2.0" << std::endl;
        exit(EXIT_SUCCESS);
    }

    std::string uuid;

    if (result.count("aws")) {
        if (!result.count("build-id")) {
            std::cerr << "No valid build ID was supplied." << std::endl;
            exit(EXIT_FAILURE);
        }

        uuid = result["build-id"].as<std::string>();
    }

    PluginManager::supportedLanguages(pluginpath, argc, argv);
    //////////////////////////////////////////////////////////////////////////////////////////
    // For each PluginManager::supported language, load the appropriate plugins
    std::string path = pluginpath.substr(0, pluginpath.find_first_of(":"));
    bool list;
    while (path.length() > 0) {
        glob_t globbuf;
        if (result.count("plugins")) {
            std::cout << "[PluMA] Current plugin list: " << std::endl;
            list = true;
        }

        for (int i = 0; i < PluginManager::supported.size(); i++)
            PluginManager::supported[i]->loadPlugin(path, &globbuf, &(PluginManager::getInstance().pluginLanguages), list);

        std::map<std::string, std::string>::iterator itr;

        for (itr = PluginManager::getInstance().pluginLanguages.begin(); itr != PluginManager::getInstance().pluginLanguages.end(); itr++)
            PluginManager::getInstance().add(itr->first.substr(0, itr->first.length()-6));
        //globfree(&globbuf);
        pluginpath = pluginpath.substr(pluginpath.find_first_of(":")+1, pluginpath.length());
        path = pluginpath.substr(0, pluginpath.find_first_of(":"));
    }

    if (list) {
        exit(EXIT_SUCCESS);
    }
    //////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////
    // Check for a restart point
    bool doRestart = false;
    std::string restartPoint = "";
    if (is_number(argv[argc - 1])) {
        doRestart = true;
        restartPoint = std::string(argv[argc - 1]);
    }
    //////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get the current time, and setup the initial log file
    time_t t = time(0);
    struct tm* now = localtime( &t );
    std::string currentTime = toString(now->tm_year + 1900) + "-" + toString(now->tm_mon + 1) + "-" + toString(now->tm_mday) + "@" + toString(now->tm_hour) + ":" + toString(now->tm_min) + ":" + toString(now->tm_sec);
    std::string mylog = "logs/"+currentTime+".log.txt";
    PluginManager::getInstance().setLogFile(mylog);

    /////////////////////////////////////////////////////////////////////
    // Read configuration file and make appropriate plugins
    readConfig(argv[argc - 2], "", doRestart, restartPoint);
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // Cleanup.
    for (int i = 0; i < PluginManager::supported.size(); i++) {
        PluginManager::supported[i]->unload();
    }
    /////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // El Finito!
    return EXIT_SUCCESS;
    /////////////////////////////////////////////////////////////////////
}
