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
   hfile << "#include \"PluginProxy.h\"" << std::endl;
   hfile << "#include <string>" << std::endl;
   hfile << std::endl;
   hfile << "class " << pluginname << "Plugin : public Plugin" << std::endl;
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
   if (!myLiteral)
      hfile << " std::map<std::string, std::string> parameters;" << std::endl;
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
      cppfile << " std::ifstream ifile(inputfile.c_str(), std::ios::in);" << std::endl;
      cppfile << " while (!ifile.eof()) {" << std::endl;
      cppfile << "   std::string key, value;" << std::endl;
      cppfile << "   ifile >> key;" << std::endl;
      cppfile << "   ifile >> value;" << std::endl;
      cppfile << "   parameters[key] = value;" << std::endl;
      cppfile << " }" << std::endl;  
   }
   cppfile << "}" << std::endl;
   cppfile << std::endl;
   cppfile << "void " << pluginname << "Plugin::run() {}" << std::endl;
   cppfile << std::endl;
   cppfile << "void " << pluginname << "Plugin::output(std::string file) {" << std::endl;
   cppfile << " std::string outputfile = file;" << std::endl;
   cppfile << " std::string myCommand = \"\";" << std::endl;
    
   cppfile << "myCommand += \"" << command[0] << "\";" << std::endl;
   cppfile << "myCommand += \" \";" << std::endl; 
   for (int i = 1; i < command.size(); i++) {
      if (myLiteral && command[i] == "inputfile")
         cppfile << "myCommand += inputfile + \" \";" << std::endl;
      else if (command[i] == "outputfile")
         cppfile << "myCommand += outputfile + \" \";" << std::endl;
      else if (command[i][0] == '-'){
         cppfile << "myCommand += \"" << command[i] << "\";" << std::endl;
         cppfile << "myCommand += \" \";" << std::endl;
      }
      else {
         cppfile << "myCommand += parameters[\"" << command[i] << "\"];" << std::endl;
         cppfile << "myCommand += \" \";" << std::endl;
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

