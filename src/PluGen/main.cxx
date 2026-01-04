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
#include <cstring>

#include "PluginGenerator.h"
#include "JavaPluginGenerator.h"

void printUsage() {
    std::cout << "Usage: ./plugen [OPTIONS] <PluginName> <command>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --lang=<language>  Generate plugin in specified language (cpp, java)" << std::endl;
    std::cout << "                     Default: cpp" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./plugen MyPlugin mycommand -i inputfile -o outputfile" << std::endl;
    std::cout << "  ./plugen --lang=java MyPlugin mycommand -i inputfile -o outputfile" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported languages:" << std::endl;
    std::cout << "  cpp   - C++ plugin (default)" << std::endl;
    std::cout << "  java  - Java plugin" << std::endl;
}

int main(int argc, char** argv) {
   // Usage will be:
   // ./plugen [--lang=<language>] <PluginName> <command, using 'inputfile' and 'outputfile' accordingly>

   if (argc < 3) {
      printUsage();
      exit(1);
   }

   std::string language = "cpp";  // Default language
   int argOffset = 1;

   // Check for --lang option
   for (int i = 1; i < argc; i++) {
       std::string arg = argv[i];
       if (arg.find("--lang=") == 0) {
           language = arg.substr(7);
           argOffset = i + 1;
           break;
       } else if (arg == "--help" || arg == "-h") {
           printUsage();
           exit(0);
       }
   }

   if (argc - argOffset < 2) {
      printUsage();
      exit(1);
   }

   std::string pluginname = std::string(argv[argOffset]); // Plugin name after options
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
   for (int i = argOffset + 2; i <= argc; i++) {
      command.push_back(std::string(argv[i-1]));
   }

   bool literal = false;
   for (size_t i = 0; i < command.size(); i++) {
      std::cout << "Command: " << command[i] << std::endl;
      if (command[i].find("inputfile") != std::string::npos) {
         literal = true;
         break;
      }
   }

   // Select generator based on language
   if (language == "java") {
      std::cout << "Generating Java plugin: " << pluginname << std::endl;
      JavaPluginGenerator* myGenerator = new JavaPluginGenerator(pluginpath, literal);
      myGenerator->generate(pluginname, command);
      delete myGenerator;
   } else if (language == "cpp") {
      std::cout << "Generating C++ plugin: " << pluginname << std::endl;
      PluginGenerator* myGenerator = new PluginGenerator(pluginpath, literal);
      myGenerator->generate(pluginname, command);
      delete myGenerator;
   } else {
      std::cerr << "Error: Unknown language '" << language << "'" << std::endl;
      std::cerr << "Supported languages: cpp, java" << std::endl;
      exit(1);
   }

   return 0;
}
