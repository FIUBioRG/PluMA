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

#include "Perl.h"
#include "../PluginManager.h"
#include "../platform.h"
#include "Py.h"   // Py::NativeStepGuard (no Python headers)
#include <mutex>
#include <stdexcept>

#ifdef HAVE_PERL
#ifdef _WIN32
#include "../bridges/PerlBridge.h"
#ifdef PLUMA_HAVE_BUILD_PATHS
#include "pluma_build_paths.h"
#endif
#else
#include "PerlRun.h"   // the Perl plugin protocol, compiled in on POSIX
#endif

// One interpreter at a time: parallel kitties must not share or clobber it.
// Never destroyed: a plugin may call exit() while other kitties hold it.
static std::mutex& perlMutex = *new std::mutex;

// PerlHost callbacks (never throw: on Windows they are called from g++ code).
static void hostLog(const char* message) {
    try { PluginManager::getInstance().log(message); } catch (...) {}
}
static int hostTakeDependencyFailed() { return PluginManager::takeDependencyFailed() ? 1 : 0; }

#ifdef _WIN32
// The runner in pluma_perl.dll (see bridges/PerlBridge.h), loaded on first
// use after Strawberry Perl's perl5XY.dll (by absolute path: PATH entries
// first, then the Perl SCons built against).  Guarded by perlMutex.
static PlumaPerlRun perlRun = NULL;
static bool perlSearched = false;
static std::string perlUnavailable;

static bool loadPerlRunner() {
    if (perlSearched) return perlRun != NULL;
    perlSearched = true;
#ifdef PLUMA_PERL_DLL
    // The Perl that pluma_perl.dll and PerlPluMA were built against first:
    // another Perl's perl5XY.dll of the same name on PATH (ActivePerl, a
    // portable Strawberry) may be built with other options, which the
    // runner's compiled-in interpreter layout would not match.  Then PATH.
    // Absolute directories only, by absolute path (never the current
    // directory), as for pythonXY.dll / jvm.dll.
    std::vector<std::string> dirs;
#ifdef PLUMA_BUILD_PERL_BIN
    dirs.push_back(PLUMA_BUILD_PERL_BIN);
#endif
    const std::vector<std::string> pathDirs = pluma::absolutePathDirs();
    dirs.insert(dirs.end(), pathDirs.begin(), pathDirs.end());
    if (!pluma::preloadDll(PLUMA_PERL_DLL, dirs)) {
        perlUnavailable = "Perl (" PLUMA_PERL_DLL ") was not found or could not be loaded; install Strawberry "
                          "Perl or put its perl\\bin on PATH";
        return false;
    }
    // (perl5XY.dll's own imports, the GCC runtime DLLs next to it, are what
    // XS modules import besides perl5XY.dll: loading Perl loaded them, so
    // those modules bind to them without Perl's bin on PATH.  Nothing else
    // in that directory is loaded.)
    std::string why;
    HMODULE bridge = pluma::loadBridge("pluma_perl.dll", "pluma_perl_bridge_version", PLUMA_PERL_BRIDGE_VERSION, why);
    PlumaPerlRun run = bridge ? (PlumaPerlRun) GetProcAddress(bridge, "pluma_perl_run") : NULL;
    if (bridge && !run) why = pluma::bridgeMismatch("pluma_perl.dll");
    if (!run) {
        perlUnavailable = "the Perl runner " + why;
        return false;
    }
    perlRun = run;
    return true;
#else
    perlUnavailable = "this build has no Perl runner";
    return false;
#endif
}
#endif
#endif

Perl::Perl(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp)
{
    // A valid argv and (empty) environment for PERL_SYS_INIT3, which may read
    // them (the runner passes each plugin's own arguments to perl_parse).
    // Static storage: PERL_SYS_INIT3 may replace the pointers it is given.
    static char arg0[] = "pluma";
    static char* args[] = { arg0, NULL };
    static char* noEnvironment[] = { NULL };
#if defined(HAVE_PERL) && !defined(_WIN32)
    static int argCount = 1;
    static char** argValues = args;
    static char** environment = noEnvironment;
    PERL_SYS_INIT3(&argCount, &argValues, &environment);
#else
    (void) args;
    (void) noEnvironment;
#endif
    // (Windows: pluma_perl.dll initializes Perl when it is first used.)
}

Perl::~Perl()
{
#if defined(HAVE_PERL) && !defined(_WIN32)
    PERL_SYS_TERM();
#endif
}

void Perl::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_PERL
    PluginManager::getInstance().log("Trying to run Perl plugin: "+pluginname+".");
    // First plugin directory that contains the script.
    const std::string filename = findPluginFile(pluginname);
    std::lock_guard<std::mutex> lock(perlMutex);
    // No GIL here: Perl steps run in parallel with Python steps on kitties.
    // A Perl plugin that calls the embedded Python (Inline::Python) takes the
    // GIL itself, as Python's C API requires on any thread -- except one that
    // started Python and kept its GIL on the main thread (see R::executePlugin).
    Py::NativeStepGuard foreignGil(false);
    const PerlHost host = { hostLog, hostTakeDependencyFailed };
    std::string message;
    int status;
#ifdef _WIN32
    if (!loadPerlRunner()) {
        PluginManager::getInstance().log("Cannot run Perl plugin " + pluginname + ": " + perlUnavailable + ".");
        throw PluginNotStarted("Perl is not available");
    }
    char text[4096] = "";
    status = perlRun(&host, pluginname.c_str(), filename.c_str(), inputname.c_str(), outputname.c_str(),
                     text, sizeof(text));
    message = text;
#else
    status = perlRunPlugin(host, pluginname, filename, inputname, outputname, message);
#endif
    if (status == PERL_RUN_NOT_STARTED) throw PluginNotStarted(message);
    if (status != PERL_RUN_OK) throw std::runtime_error(message);
#else
    notEnabled(pluginname);
#endif
}
