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

               Note: This particular file adapted from BasicUtils
               (C) 2003 Joseph Coffland under license from GNU GPL
\*********************************************************************************/

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "PluginMaker.h"
#include "PlumaExport.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>

#include "languages/Compiled.h"
#include "languages/Py.h"
#include "languages/R.h"
#include "languages/Perl.h"
#include "languages/Java.h"
#ifdef HAVE_RUST
#include "languages/Rust.h"
#endif
#ifdef HAVE_JULIA
#include "languages/Julia.h"
#endif

class PLUMA_API PluginManager {
public:
    std::map<std::string, Maker*> makers;
    std::set<std::string> installed;
    static std::vector<Language*> supported;
    std::map<std::string, std::string> pluginLanguages;

#ifdef _WIN32
    // Defined out of line in PluginManager.cxx and exported from pluma.exe:
    // an inline function-local static would be duplicated in every DLL.
    static PluginManager& getInstance();
#else
    // POSIX: plugins carry a weak copy that the dynamic linker coalesces with
    // the executable's (-rdynamic / two-level namespace on macOS), so keep it
    // inline -- plugins are linked without a reference to pluma.
    static PluginManager& getInstance()
    {
        // Must stay `static PluginManager instance` (an object): plugins built
        // against older headers share this exact function-local static with
        // the executable, and changing its type would make them misread it.
        static PluginManager instance; // Guaranteed to be destroyed.
                                       // Instantiated on first use.
        return instance;
    }
#endif

    PluginManager() : logfile(NULL) {}
    PluginManager(PluginManager const&) = delete;
    // Does not close the log file: if a plugin calls exit() while parallel
    // kitties still run, static destruction must not delete the stream they
    // are writing to.  Every line is flushed (std::endl) as it is logged.
    ~PluginManager() {}
    void operator=(PluginManager const&) = delete;

public:
    template<class T> void addMaker(std::string name, PluginMaker<T>* maker) {
        makers[name] = maker;
    }

    void add(std::string name) {installed.insert(name);}

    Plugin* create(std::string name) {
        std::map<std::string, Maker*>::iterator it = makers.find(name);
        if (it == makers.end() || it->second == NULL) return NULL;
        return it->second->create();
    }

    // Out of line (PluginManager.cxx), like dependency(): plugins call into
    // pluma's own logger rather than compiling its members into themselves.
    void setLogFile(std::string lf);
    // Whether the log file set by setLogFile() could be opened for writing.
    static bool logFileOpen();

    // Serialized: plugins on parallel "Kitty" threads log concurrently.
    static void log(std::string msg);

    // If `plugin` is not installed: on a pipeline thread, throws -- readConfig()
    // turns that into an orderly pipeline stop (also with parallel kitties).
    // On a thread the plugin started itself (std::thread, OpenMP) nothing
    // would catch it (std::terminate), so the process exits with status 1 as
    // PluMA always did -- via _Exit after flushing output (std::quick_exit is
    // missing from some macOS libcs), because exit()'s static destructors
    // would tear down the logger under kitties that are still running.
    // Defined in PluginManager.cxx (not inline: the throw stays in PluMA, so
    // plugins compiled without exceptions still build).  Where the exception
    // cannot propagate (a noexcept function or destructor, a C library
    // callback), PluMA's terminate handler ends the run the same orderly way
    // as off a pipeline thread.
    static void dependency(std::string plugin);

    // Deprecated: the most recently set prefix, shared by all threads (kept
    // for plugins compiled against older headers).  Use prefix().
    static std::string myPrefix;

    // The current pipeline "Prefix" directory.  Kept per thread: parallel
    // "Kitty" threads each run under their own prefix, and a single shared
    // string would be a data race (and give plugins a sibling's directory).
    // Defined in PluginManager.cxx.
    static void setPrefix(const std::string& prefix);
    // Sets only the calling thread's prefix (never the shared "most recent").
    static void setThreadPrefix(const std::string& prefix);

    // Whether this process is PluMA itself (set first thing in main()).  A
    // Windows multiprocessing worker (python.exe) that imports PyPluMA maps
    // pluma.exe like a DLL, and an EXE's static initializers do not run then:
    // the scripting API checks this before touching PluMA's state.
    static void markHostProcess();
    static bool inHostProcess();

    // Ends the run at once with status 1, where unwinding is not possible
    // (an unmet dependency() off a pipeline thread, exit() while kitties
    // run): removes the outputs of the steps still running that changed --
    // incomplete, like any failed step's -- except that, with
    // keepCallersOutput, the calling thread's own is moved to
    // "<output>.incomplete" (and every changed output, if it has no step of
    // its own); flushes C/C++ output; _Exit(1).  The removal is done by the
    // cleanup main() registers.
    [[noreturn]] static void abortRun(bool keepCallersOutput);
    static void setAbortCleanup(void (*cleanup)(bool keepCallersOutput));

    // Parallel kitty threads currently running, process-wide.  A kitty is
    // counted by the thread that launches it (before the thread exists) and
    // uncounted when it ends.
    static void kittyLaunching();
    static void kittyFinished();
    static int kittiesRunning();

    // POSIX: keeps the log usable in a fork()ed child (e.g. a multiprocessing
    // worker), which would otherwise inherit the log's lock held if another
    // thread was logging at the moment of fork().  Called once from main().
    static void installForkHandlers();

    // Whether the calling thread runs a pipeline (readConfig): an exception
    // thrown here normally reaches PluMA's error handling (where it cannot --
    // out of an OpenMP region, a noexcept function -- PluMA's terminate
    // handler ends the run in order).
    static void setOnPipelineThread();
    static void clearOnPipelineThread();
    // Whether the calling thread is PluMA's main thread.
    static bool isMainThread();
    // At the end of a kitty thread: frees its bookkeeping of prefix buffers
    // (not the buffers, which plugins may still hold).
    static void releaseThreadPrefixBuffers();
    // Before each step: the thread's prefix buffer, if a plugin wrote into it
    // in an earlier step, is replaced by an intact copy.
    static void refreshThreadPrefix();
    static bool onPipelineThread();

    // The per-thread mark that an unmet dependency() happened during the
    // current step (set by dependency(), cleared by readConfig() before each
    // step).  takeDependencyFailed() reads *and clears* it.  The script
    // languages use it to classify an input() that failed as "not started"
    // -- no error-text matching, which wrapping or rewording would break.
    // That classification only ever keeps an output file the step left
    // untouched (readConfig compares it with its state before the step), so
    // a plugin that caught the dependency error and then failed otherwise
    // can at most keep an earlier, unmodified result.
    static void markDependencyFailed();
    static bool takeDependencyFailed();
    // How many unmet dependency() calls this thread has had (not cleared by
    // the language runners, unlike the mark above).
    static unsigned dependencyFailureCount();
    // Whether a DependencyNotMet still exists on this thread -- one being
    // thrown, not one a plugin caught and finished handling (PluMA's
    // terminate handler; see plumaDependencyExceptionLive in Language.h).
    static bool dependencyExceptionAlive();
    static char* prefix();

    static std::string addPrefix(std::string filename) {
        return std::string(prefix())+"/"+filename;
    }

    static void supportedLanguages(
        std::string pluginpath,
        int argc,
        char** argv)
    {
        // Python, R and Perl are registered even when compiled out, so their
        // plugins fail with "<lang> support is not enabled in this build"
        // rather than being skipped.  Java is registered only when built in:
        // stray compiled *.class files must not shadow other plugins.
#if defined(_WIN32)
        supported.push_back(new Compiled("C", "dll", pluginpath, ""));
#elif defined(APPLE)
        supported.push_back(new Compiled("C", "dylib", pluginpath, "lib"));
#else
        supported.push_back(new Compiled("C", "so", pluginpath, "lib"));
#endif
        supported.push_back(new Py("Python", "py", pluginpath));
        supported.push_back(new MiAMi::R("R", "R", pluginpath, argc, argv));
        supported.push_back(new Perl("Perl", "pl", pluginpath));
#ifdef HAVE_JAVA
        supported.push_back(new Java("Java", "class", pluginpath));
#endif
#ifdef HAVE_RUST
        // Rust plugins are compiled to .so files (cdylib) but use different FFI
        supported.push_back(new Rust("Rust", "rs", pluginpath));
#endif
#ifdef HAVE_JULIA
        supported.push_back(new Julia("Julia", "jl", pluginpath));
#endif
    }

    // Whether the registered language `lang` is compiled into this build.
    static bool languageEnabled(const std::string& lang) {
        for (size_t i = 0; i < supported.size(); i++)
            if (supported[i]->lang() == lang) return supported[i]->enabled();
        return false;
    }

    static void languageLoad(std::string lang) {
        for (int i = 0; i < supported.size(); i++) {
            if (supported[i]->lang() == lang) {
                supported[i]->load();
            }
        }
    }

    static void languageUnload(std::string lang) {
        for (int i = 0; i < supported.size(); i++) {
            if (supported[i]->lang() == lang) {
                supported[i]->unload();
            }
        }
    }

private:
    std::ofstream* logfile;
    // (The log's lock is in PluginManager.cxx, never destroyed: on POSIX this
    // object is a function-local static, destroyed at exit while threads a
    // plugin started may still log.)
};

#endif
