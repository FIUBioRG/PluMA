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

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <string>
#include <map>
#include <stdexcept>
#ifndef _WIN32
#include <glob.h>   // not used here: plugins written against earlier versions get it through this header
#endif

#include "../PlumaExport.h"

// Thrown when a pipeline step fails *before the plugin's input() phase*:
// plugin not found, language not enabled, runtime unavailable, or the plugin
// failed to load (Perl compile error, Python import/construction error, Java
// class/constructor error).  Such a step never reached a phase that writes
// its output, so an existing output file -- e.g. a valid result from an
// earlier run -- is kept.  Any other exception means the plugin ran and
// failed, and its (possibly partial) output is removed.
class PluginNotStarted : public std::runtime_error {
public:
    explicit PluginNotStarted(const std::string& what) : std::runtime_error(what) {}
};

// Count the thrown DependencyNotMet objects alive per creating thread
// (defined in PluginManager.cxx).  Where the runtime no longer exposes the
// exception (MSVC calls std::terminate for a noexcept violation without it),
// PluMA's terminate handler uses this to tell an unmet dependency that could
// not propagate (the thrown object still exists) from an unrelated crash
// after a plugin caught and handled one (destroyed at the end of its catch
// block).  Only the object dependency() throws counts -- not copies a plugin
// keeps.  Created() returns the calling thread's token; Destroyed() takes the
// token of the thread that created the object, wherever it is destroyed.
PLUMA_API unsigned plumaDependencyExceptionCreated();
PLUMA_API void plumaDependencyExceptionDestroyed(unsigned creator);

// Thrown by PluginManager::dependency(): an unmet precondition, so the step
// counts as not started.  A C++ plugin that lets it propagate keeps its
// existing output; one that catches it and continues is on its own.
class DependencyNotMet : public PluginNotStarted {
public:
    explicit DependencyNotMet(const std::string& what)
        : PluginNotStarted(what), creator(plumaDependencyExceptionCreated()) {}
    // Copies (catch by value, std::make_exception_ptr) are not counted.
    DependencyNotMet(const DependencyNotMet& other) : PluginNotStarted(other), creator(0) {}
    // A move (the throw may move the temporary into the exception object)
    // hands the count over.
    DependencyNotMet(DependencyNotMet&& other) : PluginNotStarted(other), creator(other.creator) {
        other.creator = 0;
    }
    // Assignment changes the message only: each object keeps its own count.
    DependencyNotMet& operator=(const DependencyNotMet& other) {
        PluginNotStarted::operator=(other);
        return *this;
    }
    ~DependencyNotMet() { if (creator) plumaDependencyExceptionDestroyed(creator); }
private:
    unsigned creator;   // 0: a copy, not counted
};

class Language {
public:
    Language(std::string lang, std::string ext, std::string pp, std::string pre="") {language = lang; extension = ext; pluginpath = pp; prefix = pre;}
    // (No glob_t parameter: Windows has no glob(); plugin files are found
    // through platform.h on every platform.)
    virtual void loadPlugin(std::string path, std::map<std::string, std::string>* pluginLanguages, bool list = false);
    virtual void executePlugin(std::string pluginname, std::string inputfile, std::string outputfile)=0;
    virtual void unload()=0;
    virtual std::string ext() {return extension;}
    virtual std::string lang() {return language;}
    virtual std::string pre() {return prefix;}
    virtual void load()=0;
    virtual ~Language() {}
    // False when this language's support is compiled out of this build; its
    // plugins then fail with "not enabled" instead of being skipped.
    virtual bool enabled() const {return true;}
    // The shutdown PluMA does at the end of a run (earlier versions did none:
    // the process just exited).  Must not wait for threads the plugins left
    // running.  By default the same as unload().
    virtual void unloadAtExit() { unload(); }
    // Before the process ends abruptly (PluginManager::abortRun): flush the
    // language's buffered output, quickly and without waiting on anything
    // that may never come.  By default nothing.
    virtual void flushBeforeAbort() {}

protected:
    // Logs and throws: used when this language's support is compiled out, so
    // its plugins fail loudly instead of being silently skipped.
    [[noreturn]] void notEnabled(const std::string& pluginname);
    // The plugin's file, <plugin>/<plugin>Plugin.<ext>, from the plugin path
    // (first root that has it); logs and throws PluginNotStarted if there is
    // none -- before any plugin code, as for every language.
    std::string findPluginFile(const std::string& pluginname) const;

    std::string language;
    std::string extension;
    std::string prefix;
    std::string pluginpath;
};

#endif
