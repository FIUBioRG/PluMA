/********************************************************************************\

       Microbiome Analysis Using Signatures for Undocumented Microbes (MiAMi)

              Copyright (C) 2016 Bioinformatics Research Group (BioRG)
                        Florida International University

          This program is free software; you can redistribute it and/or
           modify it under the terms of the GNU General Public License
          as published by the Free Software Foundation; either version 3
              of the License, or (at your option) any later version.

         This program is distributed in the hope that it will be useful,
         but WITHOUT ANY WARRANTY; without even the implied warranty of
         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                GNU General Public License for more details.

         You should have received a copy of the GNU General Public License
            along with this program; if not, write to the Free Software
           Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
                             02110-1335, USA.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\*********************************************************************************/


#include <glob.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "Plugin.h"
#include "PluginManager.h"
#include "PluginProxy.h"
#include "Python.h"
#include "languages/Compiled.h"
#include "languages/Py.h"
#include "languages/R.h"
#include "languages/Perl.h"
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>


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



int main(int argc, char** argv) 
{
   /////////////////////////////////////////////////////////////////////////////////////////////////////////////
   // Display opening screen.
   std::cout << "***********************************************************************************" << std::endl;
   std::cout << "*                                                                                 *" << std::endl;
   std::cout << "*  PPPPPPPPPPPP    L               U           U   M           M         A        *" << std::endl;
   std::cout << "*  P           P   L               U           U   M          MM        A A       *" << std::endl;
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
   std::cout << "*    (C) 2016 Bioinformatics Research Group, Florida International University     *" << std::endl;
   std::cout << "*          Under GNU General Public License (GPL), All Rights Reserved.           *" << std::endl;
   std::cout << "*                                                                                 *" << std::endl;
   std::cout << "*    Any professionally published work using PluMA should cite the following:     *" << std::endl;
   std::cout << "*    T. Cickovski, V. Aguiar-Pulido, W. Huang, S. Mahmoud and G. Narasimhan.      *" << std::endl;
   std::cout << "*         Lightweight Microbiome Analysis Pipelines.  In Proceedings of           *" << std::endl;
   std::cout << "* 4th International Work-Conference on Bioinformatics and Biomedical Engineering  *" << std::endl;
   std::cout << "*                   (IWBBIO16), Granada, Spain, April 2016.                       *" << std::endl;  
   std::cout << "*                                                                                 *" << std::endl;
   std::cout << "***********************************************************************************" << std::endl;

   ///////////////////////////////////////////////////////////////////////////////////////////////
   // Setup plugin path.
   std::string pluginpath = "/plugins/";
   pluginpath = getenv("PWD") + pluginpath;
   if  (getenv("PLUMA_PLUGIN_PATH") != NULL) {
         pluginpath += ":";
         pluginpath += getenv("PLUMA_PLUGIN_PATH");
   }
   pluginpath += ":";
   pluginpath += "/usr/local/bin/plugins/:/usr/bin/plugins/";
   pluginpath += ":";
   std::cout << "PluginPath: " << pluginpath << std::endl;
   ///////////////////////////////////////////////////////////////////////////////////////////////

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////
   // Set supported languages here.
   std::vector<Language*> supported;
   std::map<std::string, std::string> pluginLanguages;
   supported.push_back(new Compiled("C", "so", pluginpath, "lib"));
   supported.push_back(new Py("Python", "py", pluginpath));
   supported.push_back(new MiAMi::R("R", "R", pluginpath, argc, argv));
   supported.push_back(new Perl("Perl", "pl", pluginpath));
   ///////////////////////////////////////////////////////////////////////////////////////////////

   ///////////////////////////////////////////////////////////////////////////////////////////////
   // Command line arguments
   if (argc != 2 && argc != 3 || std::string(argv[1]) == "usage") { // Usage
      std::cout << "[PluMA] Usage: ./pluma (config file) (optional restart point)" << std::endl;
      std::cout << "Arguments: help: display this message" << std::endl;
      std::cout << "           version: display release information" << std::endl;
      std::cout << "           plugins: list your installed plugins and location" << std::endl;           
      exit(0);
   }
   else if (std::string(argv[1]) == "help") { // Help
      std::cout << "[PluMA] Usage: ./pluma (config file) (optional restart point)" << std::endl;
      exit(0);
   }
   else if (std::string(argv[1]) == "version") { // Version
      std::cout << "[PluMA] Version 1.0" << std::endl;
      exit(0);
   }

   //////////////////////////////////////////////////////////////////////////////////////////
   // For each supported language, load the appropriate plugins
   std::string path = pluginpath.substr(0, pluginpath.find_first_of(":"));
   bool list = false;
   while (path.length() > 0) {
      std::cout << "Plugin Path: " << pluginpath << std::endl;
      std::cout << "Path: " << path << std::endl;
      glob_t globbuf;
      if (std::string(argv[1]) == "plugins") {
         std::cout << "[PluMA] Current plugin list: " << std::endl;
         list = true;
      }
      for (int i = 0; i < supported.size(); i++)
         supported[i]->loadPlugin(path, &globbuf, &pluginLanguages, list);
      globfree(&globbuf);
      pluginpath = pluginpath.substr(pluginpath.find_first_of(":")+1, pluginpath.length());
      path = pluginpath.substr(0, pluginpath.find_first_of(":"));
   }
   if (list) exit(0);
   //////////////////////////////////////////////////////////////////////////////////////////

   ///////////////////////////////////////////////
   // Check for a restart point
   bool doRestart = false;
   std::string restartPoint = "";
   if (argc == 3) {
      doRestart = true;
      restartPoint = std::string(argv[2]);
   }
   //////////////////////////////////////////////

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   // Get the current time, and setup the initial log file
   time_t t = time(0);
   struct tm* now = localtime( &t );
   std::string currentTime = toString(now->tm_year + 1900) + "-" + toString(now->tm_mon + 1) + "-" + toString(now->tm_mday);
   std::string mylog = "logs/"+currentTime+".log.txt";
   PluginManager::getInstance().setLogFile(mylog);
   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   
   /////////////////////////////////////////////////////////////////////
   // Read configuration file and make appropriate plugins
   std::ifstream infile(argv[1], std::ios::in);
   bool restartFlag = false;
   std::string prefix = "";
   while (!infile.eof()) {
      std::cout << "READING FILE" << std::endl;
      ///////////////////////////////////////////////////////
      // Read one line
      // Plugin (Name) inputfile (input file) outputfile (output file)
      std::string junk, name, inputname, outputname;
      infile >> junk;
      if (junk[0] == '#') {getline(infile, junk); continue;} // comment
      else if (junk == "Prefix") {infile >> prefix; prefix += "/"; continue;} // prefix
      else infile >> name >> junk >> inputname >> junk >> outputname;  
      //////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////////////////////
      // If we are restarting and have not hit that point yet, skip this plugin
      if (doRestart && !restartFlag)
         if (name == restartPoint)
            restartFlag = true;
         else
           continue;
      /////////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      // Try to create and run all three steps of the plugin in the appropriate language
      PluginManager::getInstance().log("Creating plugin "+name);
      try {
         bool executed = false;
         for (int i = 0; i < supported.size() && !executed; i++)
            if (pluginLanguages[name+"Plugin"] == supported[i]->lang()) {
               supported[i]->executePlugin(name, prefix+inputname, prefix+outputname);
               executed = true;
            }
         ///////////////////////////////////////////////////////////////////////////////////////////////////////
         // In this case we found the plugin, but the language is not supported.
         if (!executed && name != "")
             PluginManager::getInstance().log("Error, no suitable language for plugin: "+name+".");
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
   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


   /////////////////////////////////////////////////////////////////////
   // Cleanup.
   for (int i = 0; i < supported.size(); i++)
      supported[i]->unload();
   /////////////////////////////////////////////////////////////////////

   /////////////////////////////////////////////////////////////////////
   // El Finito!
   return 0;
   /////////////////////////////////////////////////////////////////////
}

