/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018 Bioinformatics Research Group (BioRG)
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

#include "Language.h"
#include <iostream>
#include <dlfcn.h>

void Language::loadPlugin(std::string path, glob_t* globbuf,
    std::map<std::string, std::string>* pluginLanguages, bool list=false)
{
    std::string pathGlob = path + "/" + "*/*" + "Plugin." + extension;
    int ext_len = extension.length()+2;
    if (glob(pathGlob.c_str(), 0, NULL, &(*globbuf)) == 0) {
        for (unsigned int i = 0; i < globbuf->gl_pathc; i++) {
            std::string filename = globbuf->gl_pathv[i];
            std::string name;
            std::string::size_type pos = filename.find_last_of("/");
            if (pos != std::string::npos) name = filename.substr(pos + prefix.length()+1, filename.length()-pos-prefix.length()-ext_len);
            else name = filename.substr(prefix.length(), filename.length()-pos-ext_len);
            if (name == "__init__") continue;
            if (!list) {
                if (extension == "so") {
                    void* handle = dlopen(filename.c_str(), RTLD_LAZY | RTLD_GLOBAL);
                    if (!handle) {
                        std::cout << "Warning: Null Handle" << std::endl;
                        std::cout << dlerror() << std::endl;
                    } else {
                        (*pluginLanguages)[name] = language;
                    }
                } else {
                    (*pluginLanguages)[name] = language;
                }
            }
        }
    } else {
        std::cout << "Found no " << language << " plugins" << std::endl;
    }
}
