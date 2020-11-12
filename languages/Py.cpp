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
#include "../PluginManager.h"
//#ifdef HAVE_PYTHON
#include "Python.h"
#include "direct.h"

//#endif

Py::Py(std::string language, std::string ext, std::string p) : Language(language, ext, p) {} 


void Py::executePlugin(std::string pluginname, std::string inputname, std::string outputname) {
//#ifdef HAVE_PYTHON
    
      char* buffer = new char[100];
      std::string cwd = _getcwd(buffer,100);
      //std::cout << "Current working directory: " << cwd << std::endl;
      //std::string temp1 = "\"";
      //std::string temp2 = temp1 + "C:\\Users\\adrie\\Desktop\\CapstoneII\\PluMA" + "\\" + "\"";
      if (!Py_IsInitialized()) Py_Initialize();
      PyRun_SimpleString("import sys,os");      
      //PyRun_SimpleString( temp2.c_str() );
      //PyRun_SimpleString(("sys.path.append(\"" + cwd + "\\" + "\")").c_str());
      PyRun_SimpleString(("sys.path.append(r\"" + cwd + "\")").c_str());
      //PyRun_SimpleString("print(sys.path)");


      std::string tmppath = pluginpath;
      #ifdef _WIN32
      std::string path = tmppath.substr(0, pluginpath.find_first_of(";"));
      #else
      std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
      #endif

      
      while (path.length() > 0) {
          //std::cout << path + pluginname << std::endl;
          #ifdef _WIN32
          PyRun_SimpleString(("sys.path.append(r\"" + path + "\\" + pluginname + "\")").c_str());
          PyRun_SimpleString(("sys.path.append(r\"" + path + "\")").c_str());
          tmppath = tmppath.substr(tmppath.find_first_of(";") + 1, tmppath.length());
          path = tmppath.substr(0, tmppath.find_first_of(";"));
          #else
          PyRun_SimpleString(("sys.path.append(\"" + path + pluginname + "/\")").c_str());
          PyRun_SimpleString(("sys.path.append(\"" + path + "/\")").c_str());
          tmppath = tmppath.substr(tmppath.find_first_of(":") + 1, tmppath.length());
          path = tmppath.substr(0, tmppath.find_first_of(":"));
          #endif
         
      }

      //std::cout << "import " + pluginname + "Plugin" << std::endl;
      PyRun_SimpleString(("import "+pluginname+"Plugin").c_str());
      PyRun_SimpleString(("plugin = "+pluginname+"Plugin."+pluginname+"Plugin()").c_str());
      PluginManager::getInstance().log("[PluMA] Executing input() For Python Plugin "+pluginname);
      //PyRun_SimpleString("print(\"Hello\")");
      PyRun_SimpleString(("plugin.input(\""+inputname+"\")").c_str());
      PluginManager::getInstance().log("[PluMA] Executing run() For Python Plugin "+pluginname);
      PyRun_SimpleString("plugin.run()");
      PluginManager::getInstance().log("[PluMA] Executing output() For Python Plugin "+pluginname);
      PyRun_SimpleString(("plugin.output(\""+outputname+"\")").c_str());
      Py_Finalize();
      PluginManager::getInstance().log("[PluMA] Python Plugin "+pluginname+" complted successfully.");
      //delete buffer;
//#endif

}

void Py::unload() {
//#ifdef HAVE_PYTHON
   if (Py_IsInitialized()) Py_Finalize();
//#endif
}
