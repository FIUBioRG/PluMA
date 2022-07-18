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

#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "PluginGenerator.hxx"

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
   for (int i = 0; i < command.size(); i++) {
      std::cout << "Command: " << command[i] << std::endl;
      if (command[i].find("inputfile") != -1) {
         literal = true;
         break;
      }
   }
   PluginGenerator* myGenerator = new PluginGenerator(pluginpath, literal);

   myGenerator->generate(pluginname, command);
}
