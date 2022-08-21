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

#include "PluginGenerator.h"
#include <dirent.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <algorithm>

// Constructor, sets the plugin path
PluginGenerator::PluginGenerator(std::string path, bool literal) {
   myPath = path;
   myLiteral = literal;
}

// Generate the plugins in the correct folder
void PluginGenerator::generate(std::string pluginname, std::vector<std::string>& command) {
   makeDirectory(pluginname);
   makeHeaderFile(pluginname);
   makeSourceFile(pluginname, command);
}

void PluginGenerator::makeDirectory(std::string pluginname) {
   std::string directory = myPath+"/"+pluginname;
   // Check if directory exists, if so return, if not make it
   DIR* dir = opendir(directory.c_str());
   if (!dir) {
      system(("mkdir "+directory).c_str());
   }
}

void PluginGenerator::makeHeaderFile(std::string pluginname) {
   std::string headerfile = myPath+"/"+pluginname+"/"+pluginname+"Plugin.h";
   std::ofstream hfile(headerfile.c_str(), std::ios::out);

   std::string str = pluginname;
   std::transform(str.begin(), str.end(),str.begin(), ::toupper);
   hfile << "#ifndef " << str << "PLUGIN_H" << std::endl;
   hfile << "#define " << str << "PLUGIN_H" << std::endl;
   hfile << std::endl;
   hfile << "#include \"Plugin.h\"" << std::endl;
   hfile << "#include \"Tool.h\"" << std::endl;
   hfile << "#include \"PluginProxy.h\"" << std::endl;
   hfile << "#include <string>" << std::endl;
   hfile << std::endl;
   hfile << "class " << pluginname << "Plugin : public Plugin, public Tool" << std::endl;
   hfile << "{" << std::endl;
   hfile << "public: " << std::endl;
   hfile << " std::string toString() {return \"" << pluginname << "\";}" << std::endl;
   hfile << " void input(std::string file);" << std::endl;
   hfile << " void run();" << std::endl;
   hfile << " void output(std::string file);" << std::endl;
   hfile << std::endl;
   hfile << "private: " << std::endl;
   hfile << " std::string inputfile;" << std::endl;
   hfile << " std::string outputfile;" << std::endl;
   //if (!myLiteral)
   //   hfile << " std::map<std::string, std::string> parameters;" << std::endl;
   hfile << std::endl;
   hfile << "};" << std::endl;
   hfile << std::endl;
   hfile << "#endif" << std::endl;
}

void PluginGenerator::makeSourceFile(std::string pluginname, std::vector<std::string>& command) {
   std::string sourcefile = myPath+"/"+pluginname+"/"+pluginname+"Plugin.cpp";
   std::ofstream cppfile(sourcefile.c_str(), std::ios::out);

   cppfile << "#include \"PluginManager.h\"" << std::endl;
   cppfile << "#include <stdio.h>" << std::endl;
   cppfile << "#include <stdlib.h>" << std::endl;
   if (!myLiteral)
      cppfile << "#include <fstream>" << std::endl;
   cppfile << "#include \"" << pluginname << "Plugin.h\"" << std::endl;
   cppfile << std::endl;
   cppfile << "void " << pluginname << "Plugin::input(std::string file) {" << std::endl;
   cppfile << " inputfile = file;" << std::endl;
   if (!myLiteral) {
      cppfile << "readParameterFile(file);" << std::endl;
      /*cppfile << " std::ifstream ifile(inputfile.c_str(), std::ios::in);" << std::endl;
      cppfile << " while (!ifile.eof()) {" << std::endl;
      cppfile << "   std::string key, value;" << std::endl;
      cppfile << "   ifile >> key;" << std::endl;
      cppfile << "   ifile >> value;" << std::endl;
      cppfile << "   parameters[key] = value;" << std::endl;
      cppfile << " }" << std::endl;
      */
   }
   cppfile << "}" << std::endl;
   cppfile << std::endl;
   cppfile << "void " << pluginname << "Plugin::run() {}" << std::endl;
   cppfile << std::endl;
   cppfile << "void " << pluginname << "Plugin::output(std::string file) {" << std::endl;
   cppfile << " std::string outputfile = file;" << std::endl;
   //cppfile << " std::string myCommand = \"\";" << std::endl;

   cppfile << "myCommand += \"" << command[0] << "\";" << std::endl;
   cppfile << "myCommand += \" \";" << std::endl;
   bool optionalflag = false;
   for (int i = 1; i < command.size(); i++) {
      if (myLiteral && command[i] == "inputfile")
         cppfile << "myCommand += inputfile + \" \";" << std::endl;
      else if (command[i] == "outputfile")
         cppfile << "myCommand += outputfile + \" \";" << std::endl;
      else if (command[i][0] == '[') {
	      optionalflag = true;
      }
      else if (command[i][0] == ']') {
              optionalflag = false;
      }
      else if (command[i][0] == '-'){
	if (command[i+1] != "inputfile" && command[i+1] != "outputfile") {
         if (optionalflag) {
              cppfile << "addOptionalParameter(\"" << command[i] << "\", \"" << command[i+1] << "\");" << std::endl;
	      i += 1;
	 }
	 else {
              cppfile << "addRequiredParameter(\"" << command[i] << "\", \"" << command[i+1] << "\");" << std::endl;
	      i += 1;
	 }
         //cppfile << "myCommand += \"" << command[i] << "\";" << std::endl;
         //cppfile << "myCommand += \" \";" << std::endl;
	}
	else {
         cppfile << "myCommand += \"" << command[i] << "\";" << std::endl;
         cppfile << "myCommand += \" \";" << std::endl;
	}
      }
      else {
         if (optionalflag) {
              cppfile << "addOptionalParameterNoFlag(\"" << command[i] << "\");" << std::endl;
	      i += 1;
	 }
	 else {
              cppfile << "addRequiredParameterNoFlag(\"" << command[i] << "\");" << std::endl;
	      i += 1;
	 }
         //cppfile << "myCommand += parameters[\"" << command[i] << "\"];" << std::endl;
         //cppfile << "myCommand += \" \";" << std::endl;
      }
   }

   cppfile << " system(myCommand.c_str());" << std::endl;
   cppfile << "}" << std::endl;

   // Proxy
   std::string proxycommand = "";
   proxycommand += "PluginProxy<";
   proxycommand += pluginname;
   proxycommand += "Plugin> ";
   proxycommand += pluginname;
   proxycommand += "PluginProxy = PluginProxy<";
   proxycommand += pluginname;
   proxycommand += "Plugin>(\"";
   proxycommand += pluginname;
   proxycommand += "\", PluginManager::getInstance());";
   cppfile << std::endl;
   cppfile << proxycommand << std::endl;
}

