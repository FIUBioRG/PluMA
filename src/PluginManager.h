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

#include <cstdlib>
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
    static std::string myPrefix;

#ifdef _WIN32
    // Defined out of line in PluginManager.cxx and exported from pluma.exe:
    // an inline function-local static would be duplicated in every DLL.
    static PluginManager& getInstance();
#else
    static PluginManager& getInstance()
    {
        static PluginManager instance; // Guaranteed to be destroyed.
                                       // Instantiated on first use.
        return instance;
    }
#endif

    PluginManager() : logfile(NULL) {}
    PluginManager(PluginManager const&) = delete;
    ~PluginManager() {
        if (logfile) delete logfile;
    }
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

    // If `plugin` is not installed: on a pipeline thread, throws
    // DependencyNotMet -- readConfig() turns that into an orderly pipeline
    // stop.  On a thread the plugin started itself (std::thread, OpenMP)
    // nothing would catch it (std::terminate), so the process exits with
    // status 1 as PluMA always did.  Defined in PluginManager.cxx (not
    // inline: the throw stays in PluMA, so plugins compiled without
    // exceptions still build).
    static void dependency(std::string plugin);

    // Whether the calling thread runs a pipeline (readConfig): an exception
    // thrown here reaches PluMA's error handling.
    static void setOnPipelineThread();
    static void clearOnPipelineThread();
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

    static char* prefix() {
        return (char*) myPrefix.c_str();
    }

    static std::string addPrefix(std::string filename) {
        return std::string(prefix())+"/"+filename;
    }

    static void supportedLanguages(
        std::string pluginpath,
        int argc,
        char** argv)
    {
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
};

#endif
