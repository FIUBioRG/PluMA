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



#include "Py.h"
#include "PluginManager.h"
#include "Python.h"


Py::Py(std::string language, std::string ext, std::string p) : Language(language, ext, p) {} 


void Py::executePlugin(std::string pluginname, std::string inputname, std::string outputname) {

      char* buffer = new char[100];
      std::string cwd = getcwd(buffer, 100);
      if (!Py_IsInitialized()) Py_Initialize();
      PyRun_SimpleString("import sys");
      PyRun_SimpleString(("sys.path.append(\""+cwd+"/\")").c_str());

      std::string tmppath = pluginpath;
      std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
      while (path.length() > 0) {
         std::cout << "Plugin Path: " << tmppath << std::endl;
         std::cout << "Python Path: " << path << std::endl;
         PyRun_SimpleString(("sys.path.append(\""+path+pluginname+"/\")").c_str());
         PyRun_SimpleString(("sys.path.append(\""+path+"/\")").c_str());
         tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
         path = tmppath.substr(0, tmppath.find_first_of(":"));
      }



      PyRun_SimpleString(("import "+pluginname+"Plugin").c_str());
      PyRun_SimpleString(("plugin = "+pluginname+"Plugin."+pluginname+"Plugin()").c_str());
      PluginManager::getInstance().log("[PluMA] Executing input() For Python Plugin "+pluginname);
      PyRun_SimpleString(("plugin.input(\""+inputname+"\")").c_str());
      PluginManager::getInstance().log("[PluMA] Executing run() For Python Plugin "+pluginname);
      PyRun_SimpleString("plugin.run()");
      PluginManager::getInstance().log("[PluMA] Executing output() For Python Plugin "+pluginname);
      PyRun_SimpleString(("plugin.output(\""+outputname+"\")").c_str());
      //Py_Finalize();
      PluginManager::getInstance().log("[PluMA] Python Plugin "+pluginname+" complted successfully.");
      delete buffer;


}

void Py::unload() {
   if (Py_IsInitialized()) Py_Finalize();
}
