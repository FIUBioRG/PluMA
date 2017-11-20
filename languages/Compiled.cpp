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


#include "Compiled.h"
#include "PluginManager.h"
#include <dlfcn.h>
#include <iostream>

Compiled::Compiled(std::string lang, std::string ext, std::string pp, std::string pre) : Language(lang, ext, pp, pre) {}

//void Compiled::loadPlugin(std::string path, glob_t* globbuf, std::map<std::string, std::string>* pluginLanguages) {
//   std::string pathGlob = path + "/" + "*/*" + "." + extension;
//   int ext_len = extension.length()+2;
//   if (glob(pathGlob.c_str(), 0, NULL, &(*globbuf)) == 0) {
//      for (unsigned int i = 0; i < globbuf->gl_pathc; i++) {
//        std::string filename = globbuf->gl_pathv[i];
//        // Get the library name
//        std::string name;
//        std::string::size_type pos = filename.find_last_of("/");
//        if (pos != std::string::npos) name = filename.substr(pos + 4, filename.length()-pos-3-ext_len);
//        else name = filename.substr(3, filename.length()-pos-ext_len);
        // Ignore if already opened.
//        std::cout << "Loading plugin " << name << "..." << std::endl;
//        void *handle = dlopen(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL);
//        if (!handle) {
//           std::cout << "Warning: Null Handle" << std::endl;
//           std::cout << dlerror() << std::endl;
//        }
//        if (handle) {
//          (*pluginLanguages)[name] = language;
//        }
//      }
//   }

//}

void Compiled::executePlugin(std::string pluginname, std::string inputname, std::string outputname) {
      std::string tmppath = pluginpath;
      std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
      std::ifstream* infile = NULL;
      std::string filename;
      do {
           if (infile) delete infile;
           filename = path+"/"+pluginname+"/lib"+pluginname+"Plugin.so";
           infile = new std::ifstream(filename, std::ios::in);
           tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
           path = tmppath.substr(0, tmppath.find_first_of(":"));
      } while (!(*infile) && path.length() > 0);// {
      // Dynamic load takes place here
      void* handle = dlopen(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL);
      if (!handle) {
            std::cout << "Warning: Null Handle" << std::endl;
            std::cout << dlerror() << std::endl;
      }
      Plugin* plugin = PluginManager::getInstance().create(pluginname);

      PluginManager::getInstance().log("Executing input() For C++/CUDA Plugin "+pluginname);
      plugin->input(inputname);
      PluginManager::getInstance().log("Executing run() For C++/CUDA Plugin "+pluginname);
      plugin->run();
      PluginManager::getInstance().log("Executing output() For C++/CUDA Plugin "+pluginname);
      plugin->output(outputname); 
      PluginManager::getInstance().log("C++/CUDA Plugin "+pluginname+" completed successfully.");
}
