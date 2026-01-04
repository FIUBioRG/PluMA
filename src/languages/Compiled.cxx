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

\********************************************************************************/

#include "Compiled.h"
#include "../PluginManager.h"
#include "../platform.h"
#include <iostream>
#include <fstream>

Compiled::Compiled(
    std::string lang,
    std::string ext,
    std::string pp,
    std::string pre
) : Language(lang, ext, pp, pre) {}

void Compiled::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
    std::string tmppath = pluginpath;
    std::string path = tmppath.substr(0, pluginpath.find_first_of(PLUMA_PATH_LIST_SEPARATOR));
    std::ifstream* infile = NULL;
    std::string filename;
    do {
        if (infile) delete infile;
        filename = path + PLUMA_PATH_SEPARATOR + pluginname + PLUMA_PATH_SEPARATOR + 
                   PLUMA_SHARED_LIB_PREFIX + pluginname + "Plugin" + PLUMA_SHARED_LIB_EXT;
        infile = new std::ifstream(filename.c_str(), std::ios::in);
        tmppath = tmppath.substr(tmppath.find_first_of(PLUMA_PATH_LIST_SEPARATOR)+1, tmppath.length());
        path = tmppath.substr(0, tmppath.find_first_of(PLUMA_PATH_LIST_SEPARATOR));
    } while (!(*infile) && path.length() > 0);
    
    delete infile;
    
    // Dynamic load takes place here
    pluma::platform::LibraryHandle handle = pluma::platform::loadLibrary(filename);
    if (!handle) {
        std::cout << "Warning: Null Handle" << std::endl;
        std::cout << pluma::platform::getLibraryError() << std::endl;
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
