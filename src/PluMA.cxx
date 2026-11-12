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

#include "PluMA.h"
#include "PluginManager.h"
#include <stdexcept>
#include <string>
using std::string;

// A freshly started process that imported a plugin module has no pipeline,
// log or prefix to talk to.  On Windows (where multiprocessing always
// spawns) the module still loads, since pluma.exe is found and mapped, and
// this check turns calls into a RuntimeError.  On POSIX a fresh python3
// cannot even load _PyPluMA (its PluginManager symbols live in the pluma
// executable), so "spawn"/"forkserver" workers -- the default on macOS, and
// on Linux from Python 3.14 -- fail at import, as they always did.  Only a
// fork()ed worker, a copy of PluMA, can use the API (sharing the log file).
static void requireHost() {
    if (!PluginManager::inHostProcess())
        throw std::runtime_error("the PluMA plugin API is only available inside the PluMA process "
                                 "(not in spawned multiprocessing workers)");
}

void log(char* msg) {
    requireHost();
    PluginManager::log(std::string(msg));
}

// Throws when the dependency is not met; the SWIG %exception handler in
// PluginWrapper.i converts that into a Python/Perl/R exception.
void dependency(char* plugin) {
    requireHost();
    PluginManager::dependency(std::string(plugin));
}

char* prefix() {
    requireHost();
    return PluginManager::prefix();
}

void languageLoad(char* lang) {
    requireHost();
    PluginManager::languageLoad(std::string(lang));
}

void languageUnload(char* lang) {
    requireHost();
    PluginManager::languageUnload(std::string(lang));
}
