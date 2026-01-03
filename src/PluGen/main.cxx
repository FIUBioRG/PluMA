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

#include "PluginGenerator.h"
#include "RustPluginGenerator.h"

void printUsage() {
    std::cout << "PluGen - PluMA Plugin Generator" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: ./plugen [options] <PluginName> <command>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --lang=<language>   Specify the target language (cpp, rust)" << std::endl;
    std::cout << "                      Default: cpp" << std::endl;
    std::cout << "  --help              Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./plugen MyPlugin mycommand -i inputfile -o outputfile" << std::endl;
    std::cout << "  ./plugen --lang=rust MyRustPlugin mycommand -i inputfile -o outputfile" << std::endl;
    std::cout << std::endl;
    std::cout << "Command syntax:" << std::endl;
    std::cout << "  inputfile    - replaced with plugin input file path" << std::endl;
    std::cout << "  outputfile   - replaced with plugin output file path" << std::endl;
    std::cout << "  -flag param  - parameter read from input parameter file" << std::endl;
    std::cout << "  [ ... ]      - optional parameters" << std::endl;
}

int main(int argc, char** argv) {
    // Parse arguments
    std::string language = "cpp";
    int argOffset = 1;

    // Check for options
    for (int i = 1; i < argc; i++) {
        std::string arg = std::string(argv[i]);
        if (arg.substr(0, 7) == "--lang=") {
            language = arg.substr(7);
            argOffset = i + 1;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            exit(0);
        } else if (arg.substr(0, 2) == "--") {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage();
            exit(1);
        } else {
            // First non-option argument
            argOffset = i;
            break;
        }
    }

    // Check minimum arguments
    if (argc - argOffset < 2) {
        printUsage();
        exit(1);
    }

    std::string pluginname = std::string(argv[argOffset]); // Plugin name
    std::string pluginpath = "../plugins/";

    // Validate language
    if (language != "cpp" && language != "rust") {
        std::cerr << "Error: Unsupported language '" << language << "'" << std::endl;
        std::cerr << "Supported languages: cpp, rust" << std::endl;
        exit(1);
    }

    std::cout << "Generating " << language << " plugin: " << pluginname << std::endl;

    // If the directory already exists, be sure they want to overwrite
    std::string directory = pluginpath + "/" + pluginname;
    DIR* dir = opendir(directory.c_str());
    if (dir) {
        std::string choice = "no";
        do {
            std::cout << "Directory " << directory << " exists. Overwrite contents (yes/no)?" << std::endl;
            std::cin >> choice;
            if (choice == "no")
                exit(0);
        } while (choice != "no" && choice != "yes");
    }

    // Build command vector
    std::vector<std::string> command;
    for (int i = argOffset + 1; i < argc; i++) {
        command.push_back(std::string(argv[i]));
    }

    // Check for literal mode (inputfile appears in command)
    bool literal = false;
    for (size_t i = 0; i < command.size(); i++) {
        std::cout << "Command: " << command[i] << std::endl;
        if (command[i].find("inputfile") != std::string::npos) {
            literal = true;
            break;
        }
    }

    // Generate plugin based on language
    if (language == "rust") {
        RustPluginGenerator* myGenerator = new RustPluginGenerator(pluginpath, literal);
        myGenerator->generate(pluginname, command);
        delete myGenerator;

        std::cout << std::endl;
        std::cout << "Rust plugin generated successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "To build:" << std::endl;
        std::cout << "  cd " << pluginpath << "/" << pluginname << std::endl;
        std::cout << "  cargo build --release" << std::endl;
        std::cout << std::endl;
        std::cout << "To install:" << std::endl;
        std::cout << "  cp target/release/lib" << pluginname << "Plugin.so $PLUMA_PLUGIN_PATH/" << pluginname << "/" << std::endl;
    } else {
        // Default: C++
        PluginGenerator* myGenerator = new PluginGenerator(pluginpath, literal);
        myGenerator->generate(pluginname, command);
        delete myGenerator;

        std::cout << std::endl;
        std::cout << "C++ plugin generated successfully!" << std::endl;
    }

    return 0;
}
