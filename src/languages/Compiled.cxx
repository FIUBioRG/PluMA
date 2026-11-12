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
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

Compiled::Compiled(
    std::string lang,
    std::string ext,
    std::string pp,
    std::string pre
) : Language(lang, ext, pp, pre) {}



// Each plugin's library (set during discovery), and whether it uses the
// Python C API -- scanned when the plugin first runs, so plugins a pipeline
// never uses cost no file reads.  Kitties ask too: guarded.  Never destroyed.
struct PluginLibrary {
    std::string path;
    int usesPython = -1;   // -1: not scanned yet
};
static std::mutex& pythonUseMutex = *new std::mutex;
static std::map<std::string, PluginLibrary>& pluginLibraries = *new std::map<std::string, PluginLibrary>;

void Compiled::setPluginLibrary(const std::string& plugin, const std::string& library) {
    std::lock_guard<std::mutex> lock(pythonUseMutex);
    PluginLibrary entry;
    entry.path = library;   // (a replaced library is scanned afresh)
    pluginLibraries[plugin] = entry;
}
bool Compiled::usesPython(const std::string& plugin) {
    std::string library;
    {
        std::lock_guard<std::mutex> lock(pythonUseMutex);
        std::map<std::string, PluginLibrary>::const_iterator found = pluginLibraries.find(plugin);
        if (found == pluginLibraries.end()) return false;
        if (found->second.usesPython >= 0) return found->second.usesPython != 0;
        library = found->second.path;
    }
    // The file reads without the lock: kitties starting other plugins do not
    // wait for this one's scan.  (Two kitties may both scan it: same answer.)
    const bool uses = Language::libraryUsesPython(library);
    std::lock_guard<std::mutex> lock(pythonUseMutex);
    std::map<std::string, PluginLibrary>::iterator found = pluginLibraries.find(plugin);
    if (found != pluginLibraries.end() && found->second.path == library) found->second.usesPython = uses ? 1 : 0;
    return uses;
}

void Compiled::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
    // The library was loaded during discovery (Language::loadPlugin), where
    // its static PluginProxy registered a maker; plugins that failed to load
    // never reach this point.
    // A plugin whose library uses the Python C API runs inside a "native
    // step": on PluMA's main thread, with the GIL held while Python is
    // running, as in earlier versions (it may call the C API relying on
    // that).  Other plugins never touch Python, so they do not hold the GIL
    // and Python steps on parallel kitties keep running meanwhile.
    // (Only the main thread's steps hold it: kitties skip the library scan.)
    Py::NativeStepGuard nativeStep(Py::onMainThread() && usesPython(pluginname), true);
    // Never deleted, as in earlier PluMA versions: existing plugins may keep
    // `this` alive past the step (detached threads, global registries) or
    // have destructors that never ran before, and a plugin DLL built with a
    // different C runtime must not be freed by pluma.exe's.
    Plugin* plugin = NULL;
    // Constructor failed (e.g. an unmet dependency() there): before input(),
    // so like other languages any existing output is kept.  Any exception
    // type; its message when it is a std::exception.
    std::string constructError;
    bool constructFailed = false;
    try {
        plugin = PluginManager::getInstance().create(pluginname);
    } catch (const std::exception& e) {
        constructFailed = true;
        constructError = std::string(": ") + e.what();
    } catch (...) {
        constructFailed = true;
    }
    if (constructFailed) {
        PluginManager::getInstance().log("C++/CUDA Plugin "+pluginname+" failed to construct"+constructError+".");
        throw PluginNotStarted("plugin " + pluginname + " failed to construct");
    }
    if (!plugin) {
        PluginManager::getInstance().log("C++/CUDA Plugin "+pluginname+" did not register itself "
                                         "(its PluginProxy keyword must be \""+pluginname+"\").");
        throw PluginNotStarted("plugin " + pluginname + " not registered");
    }

    PluginManager::getInstance().log("Executing input() For C++/CUDA Plugin "+pluginname);
    plugin->input(inputname);   // DependencyNotMet here: not started (output kept)
    // Past input(), an unmet dependency -- or any other "not started" error,
    // e.g. from a step the plugin runs itself -- is an ordinary failure: the
    // plugin may already have written partial output, which must be removed
    // (the same rule as for Python, Perl and R).
    try {
        PluginManager::getInstance().log("Executing run() For C++/CUDA Plugin "+pluginname);
        plugin->run();
        PluginManager::getInstance().log("Executing output() For C++/CUDA Plugin "+pluginname);
        plugin->output(outputname);
    } catch (const PluginNotStarted& e) {   // includes DependencyNotMet
        throw std::runtime_error(e.what());
    }
    PluginManager::getInstance().log("C++/CUDA Plugin "+pluginname+" completed successfully.");
}
