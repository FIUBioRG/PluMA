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


#ifndef PY_H
#define PY_H

#include "Language.h"


class Py : public Language {
public:
    Py(std::string language, std::string ext, std::string pp);
    void executePlugin(std::string pluginname, std::string inputname, std::string outputname);
    void unload();
    // Finalizes Python (flushes output, runs atexit handlers) -- unless a
    // non-daemon Python thread a plugin started is still running, which
    // Py_FinalizeEx would wait for forever: then only flushes the output.
    void unloadAtExit();
    // Starts the interpreter now (normally it starts on first use); see the
    // owner-thread notes in Py.cxx.
    void load();
    // Bracket a C/C++ plugin step.  On PluMA's main thread, while Python is
    // running, the step holds the GIL -- as in earlier PluMA versions, whose
    // main thread always held it -- so native plugins that call the Python C
    // API directly keep working.  No-ops elsewhere.  Defined in Py.cxx.
    static void beginNativeStep();
    static void endNativeStep();
    // beginNativeStep()/endNativeStep() for a scope -- a C/C++ step whose
    // library uses the Python C API (`active`); otherwise nothing.
    struct NativeStepGuard {
        const bool active;
        // Also when a GIL a step left held was put aside (foreignGilSaved):
        // that step's plugin -- whichever it was, the import scan may not
        // have seen it use Python -- gets it back for later steps, as all
        // native steps on the main thread did in earlier versions.
        // `nativePlugin`: a C/C++ plugin's step, which may call
        // languageLoad("Python") -- the step then holds the GIL from there on,
        // whatever the import scan said.
        explicit NativeStepGuard(bool on, bool nativePlugin = false)
            : active(on || foreignGilSaved()), native(nativePlugin) {
            if (active) beginNativeStep();
            else if (native) markNativePluginStep(true);
        }
        // Any other step too may have started Python in a way the import
        // scan cannot see (dlopen): a GIL it left held on the main thread is
        // released all the same (cheap when Python is not involved).
        ~NativeStepGuard() {
            if (active || (native && markNativePluginStep(false))) endNativeStep();
            else releaseForeignGil();
        }
        const bool native;
        NativeStepGuard(const NativeStepGuard&) = delete;
        NativeStepGuard& operator=(const NativeStepGuard&) = delete;
    };
    // Gives up the GIL if the calling thread holds it (e.g. a native step on
    // the main thread), returning what resumeGil() needs to take it back;
    // NULL when it held none.  For waiting on another language's lock: a
    // thread holding that lock may be waiting for the GIL (R's reticulate).
    // A native plugin that started Python itself on the main thread (in a
    // step, or in its static initializer while it was loaded) keeps the GIL
    // there: releases it until the next native step.  Called after each
    // native step and after plugin discovery.  Main thread only.
    static void releaseForeignGil();
    // A C/C++ step the import scan did not flag starts (true) or ends
    // (false) on the main thread; at the end, whether languageLoad("Python")
    // made it a native step (whose GIL endNativeStep() then gives back).
    static bool markNativePluginStep(bool begin);
    // Whether releaseForeignGil() put aside a GIL that is still to be taken
    // back (main thread; false elsewhere).
    static bool foreignGilSaved();
    // Whether the caller is PluMA's main thread (where native steps hold the GIL).
    static bool onMainThread();
    static void* pauseGil();
    static void resumeGil(void* paused);
    // Windows: loads pythonXY.dll by absolute path (the search Python plugins
    // use), so a C/C++ plugin DLL that links the Python C API can be loaded
    // even when Python is not on PATH.  With `stableAbi`, also python3.dll
    // (from the same directory), for plugins built against the stable ABI.
    // Returns whether it is (they are) loaded.
    static bool preloadRuntime(bool stableAbi = false);
    // Before the process ends abruptly (PluginManager::abortRun): flushes
    // Python's sys.stdout/sys.stderr if Python is running, waiting at most
    // briefly for the GIL.  No-op without Python.
    static void flushPythonBeforeAbort();
    void flushBeforeAbort() { flushPythonBeforeAbort(); }
    // Windows: the interpreter DLL's name ("python315.dll"); "" otherwise.
    static std::string runtimeDllName();
#ifdef HAVE_PYTHON
    bool enabled() const { return true; }
#else
    bool enabled() const { return false; }
#endif
};



#endif
