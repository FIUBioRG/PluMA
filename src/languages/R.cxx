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

#ifdef HAVE_R
#include "R.h"
#ifdef _WIN32
#include "../bridges/RBridge.h"
#else
#include "RInside.h"
#include "Rinterface.h"
#endif
#endif
#include "../PluginManager.h"
#include "../platform.h"
#include <stdexcept>
#if defined(HAVE_R) && defined(_WIN32) && defined(PLUMA_HAVE_BUILD_PATHS)
#include "pluma_build_paths.h"
#endif

namespace MiAMi {

#if defined(HAVE_R) && defined(_WIN32)
// Windows: R's C API in pluma_r.dll (see bridges/RBridge.h), loaded the first
// time R is needed -- pluma.exe itself does not link R, so it starts on
// machines without it.
static PlumaREval rEval = NULL;
static PlumaREnd rEnd = NULL;
static bool rRunning = false;
static bool rAttempted = false;   // (not retried after failing)

// R's installation directory: R_HOME, then the registration the R installer
// writes, then the R SCons built against.
static std::string findRHome() {
    std::vector<std::string> dirs;
    if (const char* home = getenv("R_HOME")) dirs.push_back(home);
    for (const char* key : { "SOFTWARE\\R-core\\R64", "SOFTWARE\\R-core\\R" }) {
        dirs.push_back(pluma::registryString(HKEY_CURRENT_USER, key, "InstallPath"));
        dirs.push_back(pluma::registryString(HKEY_LOCAL_MACHINE, key, "InstallPath"));
    }
#ifdef PLUMA_BUILD_R_HOME
    dirs.push_back(PLUMA_BUILD_R_HOME);
#endif
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (dirs[i].empty() || !pluma::isAbsolutePath(dirs[i])) continue;
        if (pluma::fileExists(pluma::normalizeSlashes(dirs[i]) + "/bin/x64/R.dll")) return dirs[i];
    }
    return "";
}

// Starts embedded R once; logs why if it cannot.
static bool startR(int argc, char** argv) {
    if (rRunning || rAttempted) return rRunning;
    rAttempted = true;
    const std::string home = findRHome();
    if (home.empty()) {
        PluginManager::getInstance().log("R was not found (set R_HOME, or install R for Windows).");
        return false;
    }
    // R reads R_HOME from the environment (the C runtime's and Win32's).
    _putenv_s("R_HOME", home.c_str());
    SetEnvironmentVariableA("R_HOME", home.c_str());
    // R.dll first, by absolute path (its own directory resolves its
    // dependencies), then Rblas.dll / Rlapack.dll, so package DLLs that
    // import them bind to these without R's bin on PATH; then the bridge
    // next to pluma.exe, which binds to that R.dll.
    const std::string bin = pluma::normalizeSlashes(home) + "/bin/x64";
    if (!pluma::loadLibrary(bin + "/R.dll")) {
        PluginManager::getInstance().log("R.dll could not be loaded from " + bin + ": " + pluma::lastLibraryError());
        return false;
    }
    for (const char* dll : { "Rblas.dll", "Rlapack.dll" })
        if (pluma::fileExists(bin + "/" + dll)) pluma::loadLibrary(bin + "/" + dll);
    std::string why;
    HMODULE bridge = pluma::loadBridge("pluma_r.dll", "pluma_r_bridge_version", PLUMA_R_BRIDGE_VERSION, why);
    if (!bridge) {
        PluginManager::getInstance().log("The R bridge " + why + ".");
        return false;
    }
    PlumaRStart rStart = (PlumaRStart) GetProcAddress(bridge, "pluma_r_start");
    rEval = (PlumaREval) GetProcAddress(bridge, "pluma_r_eval");
    rEnd = (PlumaREnd) GetProcAddress(bridge, "pluma_r_end");
    if (!rStart || !rEval || !rEnd) {
        PluginManager::getInstance().log("The R bridge " + pluma::bridgeMismatch("pluma_r.dll") + ".");
        return false;
    }
    char err[1024];
    if (rStart(argc, argv, err, sizeof(err)) != 0) {
        PluginManager::getInstance().log(std::string("Cannot start R: ") + err);
        return false;
    }
    rRunning = true;
    return true;
}
#endif

R::R(
    std::string language,
    std::string ext,
    std::string pp,
    int argc,
    char** argv
) : Language(language, ext, pp) {
    this->argc = argc;
    this->argv = argv;
#if defined(HAVE_R) && !defined(_WIN32)
    myR = new RInside(argc, argv);
#endif
}

void R::load() {
#ifdef HAVE_R
#ifdef _WIN32
    startR(argc, argv);
#else
    myR = new RInside(argc, argv);
#endif
#endif
}


void R::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
#ifdef HAVE_R
#ifdef _WIN32
    if (!startR(argc, argv)) {
        PluginManager::getInstance().log("R is not available; cannot run " + pluginname + ".");
        return;
    }
    // Forward slashes: the paths go into R string literals below.
    inputname = pluma::normalizeSlashes(inputname);
    outputname = pluma::normalizeSlashes(outputname);
#endif
    // The first plugin directory that has <plugin>/<plugin>Plugin.R.
    const std::string filename = pluma::findInPluginPath(pluginpath, pluginname + "/" + pluginname + "Plugin.R");
    std::ifstream* infile = new std::ifstream(filename.c_str(), std::ios::in);

    std::string txt;
    std::string line;
    while (!infile->eof()) {
        getline(*infile, line);
        txt += line+"\n";
    }
    delete infile;

    PluginManager::getInstance().log("Executing R Plugin "+pluginname);
    txt += "input(\"" + inputname + "\");\n";
    txt += "run();";
    txt += "output(\"" + outputname + "\");\n";
#ifdef _WIN32
    char err[4096];
    if (rEval(txt.c_str(), err, sizeof(err)) != 0) throw std::runtime_error(err);
#else
    myR->parseEvalQ(txt);
#endif
    PluginManager::getInstance().log("R Plugin "+pluginname+" completed successfully.");
    //unload();
#endif
}


void R::unload()
{
#ifdef HAVE_R
#ifdef _WIN32
    if (rRunning) rEnd();
    rRunning = false;
#else
    //delete myR->instancePtr();
    delete myR;

     //R_dot_Last();
     //R_RunExitFinalizers();
     //R_CleanTempDir();
     //Rf_KillAllDevices();
     //#ifndef WIN32
     //fpu_setup(FALSE);
     //#endif
     //Rf_endEmbeddedR(0);
     //RInside::instance_m = 0 ;
     //delete RInside::global_env_m;


    Rf_KillAllDevices();
    R_GlobalContext = NULL;
    Rf_endEmbeddedR(0);
#endif
#endif
}

}
