/********************************************************************************\

              Plugin-based Microbiome Analysis (PluMA, formerly MiAMi)

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


#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "PluginGenerator.h"

int main(int argc, char** argv) {
   // Usage will be:
   // ./pluginGenerate <PluginName> <command, using 'inputfile' and 'outputfile' accordingly>

   if (argc < 3) {
      std::cout << "Usage: ./pluginGenerate <PluginName> <command>"  << std::endl;
      exit(1);     
   }
   
   std::string pluginname = std::string(argv[1]); // 2nd argument is plugin name
   std::string pluginpath = "../plugins/";   

   // If the directory already exists, be sure they want to overwrite
   std::string directory = pluginpath + "/" + pluginname;
   DIR* dir = opendir(directory.c_str());
   if (dir)  {
      std::string choice = "no";
      do {
         std::cout << "Directory " << directory << " exists.  Overwrite contents (yes/no)?" << std::endl;
         std::cin >> choice;
         if (choice == "no")
            exit(0);
      } while (choice != "no" && choice != "yes");
   }

   std::vector<std::string> command;
   for (int i = 3; i <= argc; i++) {
      command.push_back(std::string(argv[i-1]));
   }   

   bool literal = false;
   for (int i = 0; i < command.size(); i++)
      if (command[i] == "inputfile") {
         literal = true;
         break;
      }
   
   PluginGenerator* myGenerator = new PluginGenerator(pluginpath, literal);   

   myGenerator->generate(pluginname, command);
}
