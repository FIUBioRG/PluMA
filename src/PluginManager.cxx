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

#include "PluginManager.h"
#include <vector>
std::vector<Language*> PluginManager::supported;
std::string PluginManager::myPrefix = "";

#ifdef _WIN32
// The plugin API as plain C, exported by pluma.exe for modules built with
// another compiler -- PerlPluMA, which must be built with Strawberry Perl's
// g++ and so cannot use PluginManager's C++ interface (see
// src/bridges/PluMAHost.cxx).
#define PLUMA_HOST_API extern "C" __declspec(dllexport)

PLUMA_HOST_API int pluma_host_api_version(void) { return 1; }

PLUMA_HOST_API void pluma_host_log(const char* message) {
    PluginManager::log(message ? message : "");
}

// As PluginManager::dependency(): ends PluMA when the dependency is not met.
PLUMA_HOST_API void pluma_host_dependency(const char* plugin) {
    PluginManager::dependency(plugin ? plugin : "");
}

PLUMA_HOST_API const char* pluma_host_prefix(void) {
    return PluginManager::prefix();
}

PLUMA_HOST_API void pluma_host_language_load(const char* lang) {
    PluginManager::languageLoad(lang ? lang : "");
}

PLUMA_HOST_API void pluma_host_language_unload(const char* lang) {
    PluginManager::languageUnload(lang ? lang : "");
}

PluginManager& PluginManager::getInstance()
{
    static PluginManager* instance = new PluginManager();
    return *instance;
}
#endif
