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
#ifdef _WIN32
#include "../bridges/RBridge.h"
#else
#include "R.h"
#include "RInside.h"
#include "Rinterface.h"
#include <R_ext/Parse.h>
// The parser's diagnostics (R's internal Parse.h; exported by libR).
extern "C" {
extern int R_ParseError;        // line of the last parse error
extern char R_ParseErrorMsg[];  // its message (NUL-terminated)
}
#endif
#endif
#include "../PluginManager.h"
#include "Py.h"
#include "../platform.h"
#include <mutex>
#include <stdexcept>
#if defined(HAVE_R) && defined(_WIN32) && defined(PLUMA_HAVE_BUILD_PATHS)
#include "pluma_build_paths.h"
#endif

namespace MiAMi {

#ifdef HAVE_R
// The embedded R interpreter is single-threaded: R plugins on parallel
// "Kitty" threads take turns.  Recursive, because an R plugin may call back
// into PluMA (RPluMA languageLoad/languageUnload) while holding it.
// Never destroyed: a plugin may call exit() while other kitties hold it.
static std::recursive_mutex& rMutex = *new std::recursive_mutex;
static int rRunning = 0;   // R plugins currently executing; guarded by rMutex

// load()/unload() requested from inside a running R plugin would replace or
// delete the interpreter under it: refuse (with a log entry) instead.
static bool rBusy(const char* what) {
    if (rRunning == 0) return false;
    PluginManager::getInstance().log(std::string("Ignoring ") + what + "(\"R\") requested while an R plugin is running.");
    return true;
}

// Embedded R cannot be restarted once Rf_endEmbeddedR() has run.
static bool rShutDown = false;   // guarded by rMutex

// The few operations PluMA needs from embedded R, all under rMutex.  POSIX:
// RInside (which disables R's C-stack check, R_CStackLimit = -1, so R can be
// called from kitty threads).  Windows: R's C API in pluma_r.dll (see
// bridges/RBridge.h), which does the same.  Errors throw std::runtime_error
// ("Error evaluating: <code>", as RInside reports them; R has printed the
// error itself, and geterrmessage() returns it).
namespace engine {
static bool running = false;
static bool attempted = false;   // start() was called (it is not retried after failing)
static std::string unavailable;   // why R could not be started (logged once per plugin)
#ifdef _WIN32
static PlumaREval rEval = NULL;
static PlumaREvalBool rEvalBool = NULL;
static PlumaREvalString rEvalString = NULL;
static PlumaRAssignString rAssignString = NULL;
static PlumaREnd rEnd = NULL;

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

static void check(int rc, const char* err) {
    if (rc != 0) throw std::runtime_error(err);
}

// R reads R_HOME from the environment (the shared UCRT one and Win32's) and
// needs it unless the R installer registered R: point it at the installation
// that will be used -- one already set may name a missing or other R that
// findRHome() passed over.  Only when R is started (a pipeline without R
// leaves the environment alone): normally at startup on the main thread,
// before any kitty runs, when R plugins are installed.  R started later on a
// kitty (languageLoad("R") there) changes it while other threads may read
// the environment -- as R itself does at startup anyway.  (PATH is never
// changed.)
static void prepareEnvironment(const std::string& home) {   // findRHome()'s answer
    const char* current = getenv("R_HOME");
    if (home.empty() || (current && home == current)) return;
    _putenv_s("R_HOME", home.c_str());
    SetEnvironmentVariableA("R_HOME", home.c_str());
}

static bool start(int argc, char** argv) {
    attempted = true;
    const std::string home = findRHome();
    if (home.empty()) {
        unavailable = "R was not found (set R_HOME, or install R for Windows)";
        return false;
    }
    const std::string bin = pluma::normalizeSlashes(home) + "/bin/x64";
    prepareEnvironment(home);
    // R.dll first, by absolute path (its own directory resolves its
    // dependencies), then Rblas.dll / Rlapack.dll, so package DLLs that
    // import them bind to these without R's bin on PATH; then the bridge
    // next to pluma.exe, which binds to that R.dll.
    if (!pluma::loadLibrary(bin + "/R.dll")) {
        unavailable = "R.dll could not be loaded from " + bin + ": " + pluma::lastLibraryError();
        return false;
    }
    for (const char* dll : { "Rblas.dll", "Rlapack.dll" })   // what package DLLs import besides R.dll
        if (pluma::fileExists(bin + "/" + dll)) pluma::loadLibrary(bin + "/" + dll);
    std::string why;
    HMODULE bridge = pluma::loadBridge("pluma_r.dll", "pluma_r_bridge_version", PLUMA_R_BRIDGE_VERSION, why);
    if (!bridge) {
        unavailable = "the R bridge " + why;
        return false;
    }
    PlumaRStart rStart = (PlumaRStart) GetProcAddress(bridge, "pluma_r_start");
    rEval = (PlumaREval) GetProcAddress(bridge, "pluma_r_eval");
    rEvalBool = (PlumaREvalBool) GetProcAddress(bridge, "pluma_r_eval_bool");
    rEvalString = (PlumaREvalString) GetProcAddress(bridge, "pluma_r_eval_string");
    rAssignString = (PlumaRAssignString) GetProcAddress(bridge, "pluma_r_assign_string");
    rEnd = (PlumaREnd) GetProcAddress(bridge, "pluma_r_end");
    if (!rStart || !rEval || !rEvalBool || !rEvalString || !rAssignString || !rEnd) {
        unavailable = "the R bridge " + pluma::bridgeMismatch("pluma_r.dll");
        return false;
    }
    char err[1024];
    if (rStart(argc, argv, err, sizeof(err)) != 0) {
        unavailable = err;
        return false;
    }
    running = true;
    return true;
}
static void evalQ(const std::string& code) {
    char err[4096];
    check(rEval(code.c_str(), err, sizeof(err)), err);
}
static bool evalBool(const std::string& code) {
    char err[4096];
    int value = 0;
    check(rEvalBool(code.c_str(), &value, err, sizeof(err)), err);
    return value != 0;
}
static std::string evalString(const std::string& code) {
    char err[4096];
    std::vector<char> buf(64 * 1024);
    check(rEvalString(code.c_str(), buf.data(), (int) buf.size(), err, sizeof(err)), err);
    return std::string(buf.data());
}
static void assign(const std::string& name, const std::string& value) {
    char err[1024];
    check(rAssignString(name.c_str(), value.c_str(), err, sizeof(err)), err);
}
static void end() {
    if (running) rEnd();
    running = false;
}
#else
static RInside* myR = NULL;
static bool start(int argc, char** argv) {
    attempted = true;
    // An R_HOME that is not an R installation (e.g. left from an R since
    // removed) makes R exit the whole process at startup ("unable to open
    // the base package").  Ignore it, as on Windows: RInside then uses the
    // R it was built against.
    if (const char* home = getenv("R_HOME")) {
        if (*home && !pluma::isDirectory(std::string(home) + "/library/base")) {
            const std::string note = std::string("R_HOME=") + home +
                                     " is not an R installation; using the R PluMA was built with.";
            std::cerr << "[PluMA] " << note << std::endl;   // also on stderr: the log may not be open yet
            PluginManager::getInstance().log(note);
            unsetenv("R_HOME");
        }
    }
    // RInside throws when R cannot be set up: R
    // plugins then fail with this reason, other plugins still run.
    try {
        myR = new RInside(argc, argv);
    } catch (const std::exception& e) {
        unavailable = e.what();
        return false;
    } catch (...) {
        unavailable = "RInside could not initialize R";
        return false;
    }
    running = true;
    return true;
}
// R_ParseVector run under R_ToplevelExec: the parser can raise an R error
// (e.g. "invalid multibyte character", a script not saved as UTF-8) rather
// than return a status, which RInside does not catch -- R would end the
// process ("Execution halted").  R prints the message.
struct ParseCheck {
    SEXP source;
    ParseStatus status;
};
static void parseCheck(void* data) {
    ParseCheck* check = static_cast<ParseCheck*>(data);
    R_ParseVector(check->source, -1, &check->status, R_NilValue);
}
static void evalQ(const std::string& code) {
    SEXP source = PROTECT(Rf_mkString(code.c_str()));
    ParseCheck check = { source, PARSE_NULL };
    const bool parsed = R_ToplevelExec(parseCheck, &check);
    UNPROTECT(1);
    if (!parsed)
        throw std::runtime_error("Parse error: R could not read the script (e.g. characters not valid in UTF-8; "
                                 "save it as UTF-8)");
    // Not handed to RInside then: it takes an incomplete expression for one
    // continued on the next call and keeps the text, prepending it to every
    // later evaluation.  Reported as the Windows bridge does.
    if (check.status != PARSE_OK)
        throw std::runtime_error("Parse error at line " + std::to_string(R_ParseError) + ": " +
                                 (check.status == PARSE_INCOMPLETE ? std::string("incomplete expression at end of input")
                                                                   : std::string(R_ParseErrorMsg)));
    myR->parseEvalQ(code);
}
static bool evalBool(const std::string& code) { return Rcpp::as<bool>(myR->parseEval(code)); }
static std::string evalString(const std::string& code) { return Rcpp::as<std::string>(myR->parseEval(code)); }
static void assign(const std::string& name, const std::string& value) { (*myR)[name] = value; }
static void end() {
    // RInside's destructor shuts embedded R down itself (.Last, exit
    // finalizers, temp-dir cleanup, Rf_endEmbeddedR); ending R a second time
    // here would re-run that on a finalized interpreter.  A plugin's .Last
    // is run first, here, with errors caught (and then removed, so it does
    // not run twice): an error in it during RInside's shutdown ends PluMA
    // with status 1 ("Execution halted") -- as on Windows (RBridge.cxx).
    try {
        myR->parseEvalQ("base::tryCatch(base::local({ if (base::exists('.Last', envir = base::globalenv(), "
                        "mode = 'function', inherits = FALSE)) { last <- base::get('.Last', "
                        "envir = base::globalenv()); base::rm('.Last', envir = base::globalenv()); "
                        "last() } }), error = function(e) base::message('Error in .Last: ', "
                        "base::conditionMessage(e)))");
    } catch (...) {}
    delete myR;
    myR = NULL;
    running = false;
}
#endif
}  // namespace engine

// R's package loading (dyn.load) calls SetDllDirectory(NULL), which puts the
// current directory back on the DLL search path; main() takes it off (DLL
// planting).  Taken off again after anything that may load packages.
struct KeepDllSearchHardened {
    ~KeepDllSearchHardened() {
#ifdef _WIN32
        SetDllDirectoryA("");
#endif
    }
};

// Starts embedded R and makes quit()/q() end only the calling plugin, as
// sys.exit() and Perl's exit() do: R's own quit() would end PluMA itself
// (R_CleanUp calls exit()), skipping the rest of the pipeline.  The
// replacement records the status in an R option and signals an error,
// which executePlugin() classifies.  (Its environment is base, so a plugin's
// own definitions of stop() or options() do not affect it.)
static bool startR(int argc, char** argv) {
    KeepDllSearchHardened keepDllSearchHardened;
    if (!engine::start(argc, argv)) return false;
    try {
        engine::evalQ(
            "local({\n"
            "  quitPlugin <- function(save = 'default', status = 0, runLast = TRUE) {\n"
            "    base::options(pluma.quit.status = base::as.integer(status))\n"
            // Unhandled, stop() below still prints as an error: silenced
            // until PluMA has dealt with the quit (clearRQuit).
            "    if (base::is.null(base::getOption('pluma.quit.show')))\n"
            "      base::options(pluma.quit.show = base::getOption('show.error.messages', TRUE))\n"
            "    base::options(show.error.messages = FALSE)\n"
            // Not an error condition: try() and tryCatch(error = ...) do not
            // catch it (R's own quit() cannot be caught at all); unhandled,
            // stop() still ends the evaluation with an error.
            "    base::stop(base::structure(base::list(message = 'quit() ends this PluMA plugin', call = NULL),\n"
            "                               class = base::c('plumaQuit', 'condition')))\n"
            "  }\n"
            "  base::environment(quitPlugin) <- base::baseenv()\n"
            "  for (env in base::list(base::baseenv(), base::asNamespace('base')))\n"
            "    for (name in base::c('quit', 'q')) {\n"
            "      base::unlockBinding(name, env)\n"
            "      base::assign(name, quitPlugin, envir = env)\n"
            "      base::lockBinding(name, env)\n"
            "    }\n"
            "})");
    } catch (const std::exception& e) {
        // Also on stderr: on POSIX this runs before the log file is open.
        const std::string warning = std::string("WARNING: R's quit() could not be redirected (") + e.what() +
                                    "); an R plugin calling quit() ends PluMA.";
        std::cerr << "[PluMA] " << warning << std::endl;
        PluginManager::getInstance().log(warning);
    }
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
    // POSIX: started here, with PluMA and before plugin libraries are loaded,
    // as always -- C/C++ plugins may use the process's RInside directly, also
    // from their static initializers.  (Windows: main() starts it when R
    // plugins are installed; otherwise load() or the first R step does.)
    startR(argc, argv);
#endif
}

// Taking rMutex while holding the GIL (a native step on the main thread
// calling languageLoad/Unload) could deadlock with an R step on a kitty that
// holds rMutex and waits for the GIL (reticulate): the GIL is given up while
// waiting -- R steps themselves take rMutex first, then the GIL.
#ifdef HAVE_R
static std::unique_lock<std::recursive_mutex> lockRWithoutGil() {
    void* gil = Py::pauseGil();
    std::unique_lock<std::recursive_mutex> lock(rMutex);
    Py::resumeGil(gil);
    return lock;
}
#endif

void R::load() {
#ifdef HAVE_R
    std::unique_lock<std::recursive_mutex> lock = lockRWithoutGil();
    if (engine::running) return;   // already running (one R per process): silent no-op
    if (rBusy("languageLoad")) return;
    if (rShutDown) {
        PluginManager::getInstance().log("Ignoring languageLoad(\"R\"): R was shut down earlier and cannot be restarted.");
        return;
    }
    // Embedded R cannot be initialized twice: a start that failed is final.
    if (engine::attempted) {
        PluginManager::getInstance().log("R is not available: it could not be started (" + engine::unavailable +
                                         "); R plugins will not run.");
        return;
    }
    if (!startR(argc, argv))
        PluginManager::getInstance().log("Cannot start R: " + engine::unavailable + ".");
#endif
}


#ifdef HAVE_R
// Forgets a quit() by the plugin, and lets R print errors again.
static void clearRQuit() {
    try {
        engine::evalQ("base::options(show.error.messages = base::getOption('pluma.quit.show', "
                      "base::getOption('show.error.messages')), pluma.quit.show = NULL, pluma.quit.status = NULL)");
    } catch (...) {}
}

// The R error behind a failed evaluation: R's own message (the exception's
// text may be just the evaluated source, whose length it is cut to), or the
// parse error PluMA reported, which has the line and cause.
static std::string rFailureText(const std::exception& e) {
    std::string why = e.what();
    if (why.compare(0, 11, "Parse error") == 0) return why;
    std::string rError;
    try { rError = engine::evalString("base::geterrmessage()"); } catch (...) {}
    while (!rError.empty() && (rError.back() == '\n' || rError.back() == ' ')) rError.pop_back();
    if (!rError.empty()) return rError;
    return why.size() > 300 ? why.substr(0, 300) + "..." : why;
}
#endif

void R::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
#ifdef HAVE_R
    // First plugin directory that contains the script.
    const std::string filename = findPluginFile(pluginname);
    std::ifstream infile(filename.c_str(), std::ios::in);
    if (!infile.is_open()) {
        PluginManager::getInstance().log("R plugin "+pluginname+" could not be read: "+filename);
        throw PluginNotStarted("R plugin " + pluginname + " could not be read");
    }

    std::string raw;
    pluma::readWholeStream(infile, raw);
    infile.close();
    // Any line endings -- Windows' CRLF, old Mac CR, a doubled CR from a
    // repeated conversion -- become '\n': R's parser rejects a '\r'
    // ("unexpected input") where the C runtime does not strip it.  Ends in a
    // newline, as R's own source() reads it.
    std::string txt;
    txt.reserve(raw.size() + 1);
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '\r') txt += raw[i];
        else if (i + 1 == raw.size() || raw[i + 1] != '\n') txt += '\n';   // (CR before LF: the LF is the end)
    }
    if (txt.empty() || txt.back() != '\n') txt += '\n';
    // A script saved as "UTF-8 with BOM": R's parser rejects the mark.
    pluma::stripUtf8Bom(txt);

    std::lock_guard<std::recursive_mutex> lock(rMutex);
    // No GIL here: R steps run in parallel with Python steps on kitties.  An
    // R plugin that calls the embedded Python (reticulate) takes the GIL
    // itself, as Python's C API requires on any thread (PyGILState_Ensure).
    // Except one that started Python and kept its GIL on the main thread:
    // given back after the step, and taken again for later steps there
    // (see Py::releaseForeignGil) -- after rMutex, the order R steps use.
    Py::NativeStepGuard foreignGil(false);
    KeepDllSearchHardened keepDllSearchHardened;
    // Not started yet (no R plugin was installed when PluMA started, e.g. a
    // plugin path changed since): start it now, once.
    if (!engine::running && !rShutDown && !engine::attempted) startR(argc, argv);
    if (!engine::running) {
        if (rShutDown)
            PluginManager::getInstance().log("R was shut down (languageUnload); cannot run R plugin "+pluginname+".");
        else
            PluginManager::getInstance().log("Cannot start R: " + engine::unavailable + "; cannot run R plugin " +
                                             pluginname + ".");
        throw PluginNotStarted("R is not running");
    }
    struct Running {   // rRunning while this plugin executes, also on throw
        Running() { ++rRunning; }
        ~Running() { --rRunning; }
    } running;
    PluginManager::getInstance().log("Executing R Plugin "+pluginname);
    // Load the script (its function definitions) first: a syntax or load
    // error there is before input(), so -- like a Perl compile error or a
    // Python import error -- any existing output is kept.
    // All R plugins share R's global environment: first remove the previous
    // plugin's input/run/output, so a plugin that does not define one of
    // them never silently runs another plugin's.
    try {
        engine::evalQ("base::rm(list = base::intersect(base::c('input', 'run', 'output'), base::ls(base::globalenv())), envir = base::globalenv())");
        engine::evalQ(txt);
    } catch (const std::exception& e) {
        const std::string why = rFailureText(e);   // (before clearRQuit, which evaluates)
        clearRQuit();   // (a script that called quit() while loading)
        PluginManager::getInstance().log("R Plugin "+pluginname+" failed to load: "+why);
        throw PluginNotStarted("R plugin " + pluginname + " failed to load");
    }
    // Which phases the plugin defines is decided once, right after loading:
    // a plugin whose input() clears the global environment must then fail
    // when run()/output() are called, not have them skipped as "optional".
    // One evaluation for all three ("110": input and run defined).  Functions
    // only: a variable that happens to be called output is not a phase.
    bool hasInput = false, hasRun = false, hasOutput = false;
    try {
        const std::string defined = engine::evalString(
            "base::paste0(base::as.integer(base::vapply(base::c('input', 'run', 'output'), "
            "function(f) base::exists(f, envir = base::globalenv(), mode = 'function', inherits = FALSE), "
            "base::logical(1))), "
            "collapse = '')");
        if (defined.size() != 3) throw std::runtime_error("unexpected answer: " + defined);
        hasInput = defined[0] == '1';
        hasRun = defined[1] == '1';
        hasOutput = defined[2] == '1';
    } catch (const std::exception& e) {   // e.g. the script shadowed exists()
        PluginManager::getInstance().log("R Plugin "+pluginname+" failed to load: "+e.what());
        throw PluginNotStarted("R plugin " + pluginname + " failed to load");
    }
    if (!hasInput) {   // nothing has run yet (as for Python and Java)
        PluginManager::getInstance().log("R Plugin "+pluginname+" has no input() function.");
        throw PluginNotStarted("R plugin " + pluginname + " has no input() function");
    }
    // Paths are passed as R variables, not pasted into R source, so quotes
    // or backslashes in them cannot break (or inject into) the script.  Each
    // is assigned right before its call, so a plugin that clears or reuses
    // the global environment in input()/run() cannot change it.
    // A phase that called quit(): status 0 ends the plugin successfully (the
    // remaining phases are skipped), as sys.exit(0) / Perl's exit(0) do; any
    // other status is a failure.  Returns true if the plugin quit with 0.
    auto quitStatus = [&](const char* phase) -> bool {
        bool quit = false;
        try { quit = engine::evalBool("!base::is.null(base::getOption('pluma.quit.status'))"); } catch (...) {}
        if (!quit) return false;
        int status = 1;
        try { status = std::stoi(engine::evalString("base::as.character(base::getOption('pluma.quit.status'))")); }
        catch (...) {}
        clearRQuit();
        PluginManager::getInstance().log("R Plugin " + pluginname + " called quit(status = " + std::to_string(status) +
                                         ") during " + phase + "().");
        if (status == 0) return true;
        throw std::runtime_error("R plugin " + pluginname + " quit with status " + std::to_string(status));
    };
    // Runs one phase; false if the plugin quit(0) (the rest is skipped).
    auto runPhase = [&](const char* phase, const std::string& call) -> bool {
        // Cleared before each phase, so a status always belongs to this phase.
        clearRQuit();
        try {
            engine::evalQ(call);   // throws on an R error
        } catch (const std::exception& e) {
            if (quitStatus(phase)) return false;
            PluginManager::getInstance().log("R Plugin " + pluginname + " " + phase + "() failed: " + rFailureText(e));
            // input() failing because of an unmet RPluMA dependency(): not started.
            if (std::string(phase) == "input" && PluginManager::takeDependencyFailed()) {
                PluginManager::getInstance().log("R Plugin "+pluginname+" input() failed: unmet dependency.");
                throw PluginNotStarted("R plugin " + pluginname + " has an unmet dependency");
            }
            throw;
        }
        // The phase returned, but called quit() and intercepted it anyway
        // (tryCatch(condition = ...)): it still counts, as R's own quit()
        // cannot be caught.
        return !quitStatus(phase);
    };
    // Before input(): a failure here means the plugin never started (its
    // existing output is kept, as for a script that fails to load).
    try {
        engine::assign(".pluma_input", inputname);
    } catch (const std::exception& e) {
        PluginManager::getInstance().log("R Plugin " + pluginname + ": could not pass the input path to R: " + e.what());
        throw PluginNotStarted("R plugin " + pluginname + " could not be given its input path");
    }
    // The paths go in as values (a call built with them, evaluated in the
    // global environment), not as R variables the plugin could remove before
    // its lazily evaluated argument is first used.
    bool goOn = runPhase("input", "base::eval(base::call('input', .pluma_input), base::globalenv())");
    // run()/output() are optional, as for Python, Java and the C++ base class.
    if (goOn) {
        if (hasRun) goOn = runPhase("run", "run()");
        else PluginManager::getInstance().log("R Plugin "+pluginname+" has no run(); skipped.");
    }
    if (goOn) {
        if (hasOutput) {
            try {
                engine::assign(".pluma_output", outputname);
            } catch (const std::exception& e) {
                PluginManager::getInstance().log("R Plugin " + pluginname + ": could not pass the output path to R: " +
                                                 e.what());
                throw;
            }
            runPhase("output", "base::eval(base::call('output', .pluma_output), base::globalenv())");
        } else PluginManager::getInstance().log("R Plugin "+pluginname+" has no output(); skipped.");
    }
    PluginManager::getInstance().log("R Plugin "+pluginname+" completed successfully.");
    //unload();
#else
    notEnabled(pluginname);
#endif
}


void R::unload()
{
#ifdef HAVE_R
    std::unique_lock<std::recursive_mutex> lock = lockRWithoutGil();
    if (rBusy("languageUnload")) return;
    if (!engine::running) return;   // not started, or already shut down (languageUnload, then exit cleanup)
    engine::end();
    rShutDown = true;
#endif
}

}
