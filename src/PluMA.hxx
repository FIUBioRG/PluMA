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

#ifndef _PLUMA_H
#define _PLUMA_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include "PluginManager.h"

using namespace std;
namespace fs = std::filesystem;

class PluMA
{
public:
    string pluginpath;

    static void log(char *msg)
    {
        PluginManager::log(std::string(msg));
    }

    static void dependency(char *plugin)
    {
        PluginManager::dependency(std::string(plugin));
    }

    static char *prefix()
    {
        return PluginManager::prefix();
    }

    static void languageLoad(char *lang)
    {
        PluginManager::languageLoad(std::string(lang));
    }

    static void languageUnload(char *lang)
    {
        PluginManager::languageUnload(std::string(lang));
    }

    PluMA();
    void read_config(char *inputfile, string prefix, bool doRestart, char *restartPoint);

private:
    string plugin_path;
    map<string, fs::path> *buildfiles;
};
#endif
