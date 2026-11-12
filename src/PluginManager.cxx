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
#include <cstdio>
#include <mutex>
#include <vector>
std::vector<Language*> PluginManager::supported;
std::string PluginManager::myPrefix = "";

// Never destroyed (unlike this object on POSIX, a function-local static):
// a thread a plugin started may log while the process exits.
// (Function-local: usable even from another file's static initializer.)
static std::mutex& logMutex() {
    static std::mutex* lock = new std::mutex;
    return *lock;
}

void PluginManager::setLogFile(std::string lf) {
    std::lock_guard<std::mutex> lock(logMutex());
    if (logfile) delete logfile;
    logfile = new std::ofstream(lf.c_str(), std::ios::out);
}

bool PluginManager::logFileOpen() {
    PluginManager& self = getInstance();
    std::lock_guard<std::mutex> lock(logMutex());
    return self.logfile && self.logfile->is_open();
}

void PluginManager::log(std::string msg) {
    PluginManager& self = getInstance();
    std::lock_guard<std::mutex> lock(logMutex());
    if (self.logfile) *(self.logfile) << "[PluMA] " << msg << std::endl;
}

static thread_local bool dependencyFailed = false;
static thread_local unsigned dependencyFailures = 0;   // on this thread, never cleared

void PluginManager::markDependencyFailed() {
    dependencyFailed = true;
    ++dependencyFailures;
}

unsigned PluginManager::dependencyFailureCount() {
    return dependencyFailures;
}

bool PluginManager::takeDependencyFailed() {
    const bool failed = dependencyFailed;
    dependencyFailed = false;
    return failed;
}

static thread_local bool pipelineThread = false;

void PluginManager::setOnPipelineThread() {
    pipelineThread = true;
}

void PluginManager::clearOnPipelineThread() {
    pipelineThread = false;
}

bool PluginManager::onPipelineThread() {
    return pipelineThread;
}

void PluginManager::dependency(std::string plugin) {
    if (getInstance().installed.count(plugin) == 0) {
        log("Plugin dependency " + plugin + " not met.");
        if (!onPipelineThread()) {
            // Also on stderr: the log file may not be open yet (e.g. called
            // from a plugin's static initializer during discovery).
            const std::string message = "[PluMA] Plugin dependency " + plugin +
                                        " not met outside a pipeline step; exiting.\n";
            std::fputs(message.c_str(), stderr);
            std::fflush(stderr);
            exit(1);
        }
        // C++ plugins get DependencyNotMet; through the SWIG wrappers it
        // becomes a Python/Perl/R error carrying the same message.  The mark
        // records that it really happened on this thread (see
        // takeDependencyFailed()).
        markDependencyFailed();
        throw DependencyNotMet("plugin dependency " + plugin + " not met");
    } else {
        log("Plugin dependency " + plugin + " met.");
    }
}

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

// 0 if `plugin` is installed; otherwise 1 with the error text in `err` (the
// caller turns it into an exception of its own language).  Off a pipeline
// thread an unmet dependency ends the run, as PluginManager::dependency does.
PLUMA_HOST_API int pluma_host_dependency(const char* plugin, char* err, int errlen) {
    try {
        PluginManager::dependency(plugin ? plugin : "");
        return 0;
    } catch (const std::exception& e) {
        if (err && errlen > 0) snprintf(err, (size_t) errlen, "%s", e.what());
        return 1;
    } catch (...) {
        if (err && errlen > 0) snprintf(err, (size_t) errlen, "%s", "dependency check failed");
        return 1;
    }
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
