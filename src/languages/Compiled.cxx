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

#include "Compiled.h"
#include "../PluginManager.h"
#include "../platform.h"
#include <iostream>

Compiled::Compiled(
    std::string lang,
    std::string ext,
    std::string pp,
    std::string pre
) : Language(lang, ext, pp, pre) {}

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

void Compiled::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
    // The plugin's library, <plugin>/<prefix><plugin>Plugin.<ext>, from the
    // first plugin directory that has it ("lib" prefix on POSIX, none on
    // Windows).  It was loaded during discovery; loading it again returns the
    // same module.
    std::string filename = pluma::findInPluginPath(pluginpath, pluginname + "/" + pre() + pluginname + "Plugin." + ext());
    void* handle = filename.empty() ? NULL : pluma::loadLibrary(filename);
    if (!handle) {
        std::string err = pluma::lastLibraryError();   // before any I/O
        std::cout << "Warning: Null Handle" << std::endl;
        std::cout << err << std::endl;
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
