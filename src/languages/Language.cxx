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

#include "Language.h"
#include "../platform.h"
#include <iostream>

void Language::loadPlugin(
    std::string path,
    std::map<std::string, std::string>* pluginLanguages,
    bool list
) {
    // Matches <path>/*/*Plugin.<ext>
    std::vector<std::string> files = pluma::findPluginFiles(path, extension);
    const std::string::size_type ext_len = extension.length()+2;
    if (!files.empty()) {
        for (size_t i = 0; i < files.size(); i++) {
            std::string filename = pluma::normalizeSlashes(files[i]);
            std::string name;
            std::string::size_type pos = filename.find_last_of("/");
            if (pos != std::string::npos) {
                name = filename.substr(pos + prefix.length()+1, filename.length()-pos-prefix.length()-ext_len);
            } else {
                name = filename.substr(prefix.length(), filename.length()-pos-ext_len);
            }
            if (name == "__init__") continue;
            //std::cout << "Plugin: " << name.substr(0, name.length()-6) << " Language: " << language << " Path: " << path << std::endl;
            if (!list) {
                if (extension == "so" || extension == "dll" || extension == "dylib") {
                    void* handle = pluma::loadLibrary(filename);
                    if (!handle) {
                        std::string err = pluma::lastLibraryError();   // before any I/O
                        std::cout << "Warning: Null Handle" << std::endl;
                        std::cout << err << std::endl;
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
