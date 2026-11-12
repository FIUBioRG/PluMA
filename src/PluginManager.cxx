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
#include "platform.h"
#ifndef _WIN32
#include <pthread.h>       // pthread_atfork
#endif
#include <iostream>
#include <string>
#include <atomic>
#include <cstring>
#include <map>
#include <set>
#include <vector>
std::vector<Language*> PluginManager::supported;
// Every prefix value prefix() hands out, kept for the whole run (never
// freed), so a pointer a plugin got from prefix() stays valid after the
// prefix changes and after the thread that got it ends -- e.g. passed to a
// detached thread or a global registry, as the old process-wide prefix
// allowed.  prefix() returns char*, and a plugin may write into its buffer
// (strtok, as into the old one): that changes only what its own thread
// sees, until the prefix is set again -- never the lookup (the buffer is a
// copy, not the map's key) -- and the thread's next step gets the real
// prefix again (refreshThreadPrefix).  Pipeline threads (main, kitties) get
// buffers of their own, never handed to another thread, so a write never
// reaches another kitty (a kitty's buffers stay with whatever still holds
// them after it ends: a few bytes per kitty).  Threads a plugin starts
// itself (any number of them) share one buffer per prefix: a write into it reaches the other such
// threads holding that pointer, until their next prefix() call, which hands
// out a fresh copy (see prefix()).  Never destroyed: the buffers outlive
// their thread, and exit() may come under running kitties.
static std::mutex& sharedInternMutex = *new std::mutex;
// A pipeline thread's buffers (a plain pointer: no thread_local destructor,
// which could run before a plugin's own thread_local destructors that still
// call into PluMA); handed back by PluginManager::releaseThreadPrefixBuffers.
static thread_local std::map<std::string, char*>* ownPrefixBuffers = NULL;
static char* copyPrefix(const std::string& prefix) {
    char* buffer = new char[prefix.size() + 1];
    std::memcpy(buffer, prefix.c_str(), prefix.size() + 1);
    return buffer;
}
static const char* internPrefix(const std::string& prefix, bool ownBuffer) {
    static std::map<std::string, char*>& shared = *new std::map<std::string, char*>;
    std::lock_guard<std::mutex> lock(sharedInternMutex);
    if (ownBuffer) {
        if (!ownPrefixBuffers) ownPrefixBuffers = new std::map<std::string, char*>;
        char*& buffer = (*ownPrefixBuffers)[prefix];
        // (One a plugin wrote into is left to it, still valid: a fresh one.)
        if (!buffer || std::strcmp(buffer, prefix.c_str()) != 0) buffer = copyPrefix(prefix);
        return buffer;
    }
    char*& buffer = shared[prefix];
    if (!buffer || std::strcmp(buffer, prefix.c_str()) != 0) buffer = copyPrefix(prefix);
    return buffer;
}
// The prefix strings fallback threads compare their shared buffer against
// (one per distinct prefix; never destroyed, so a thread_local pointer into
// it needs no destructor).
static const std::string* expectedPrefix(const std::string& prefix) {
    static std::set<std::string>& strings = *new std::set<std::string>;
    std::lock_guard<std::mutex> lock(sharedInternMutex);
    return &*strings.insert(prefix).first;
}
// See PluginManager::setPrefix(): one prefix per pipeline thread.
static thread_local const char* currentPrefix = NULL;   // interned
static thread_local const std::string* currentPrefixName = NULL;   // what it holds (see expectedPrefix)
// For threads that never ran a pipeline: the interned "most recent" prefix
// as of this thread's last prefix() call.
static thread_local const char* fallbackCurrent = NULL;
static thread_local unsigned fallbackVersion = 0;
// The prefix fallbackCurrent was interned for: its buffer is shared with the
// other such threads, and one of them may have written into it.
static thread_local const std::string* fallbackExpected = NULL;   // see expectedPrefix()
// Bumped on every setPrefix(); lets fallback threads skip the lock when the
// prefix has not changed since their last call.
static std::atomic<unsigned> prefixVersion(1);

// "The most recently set prefix" across all threads, which is what PluMA's
// prefix always meant before it became per thread.  It is the fallback for
// threads that never ran a pipeline (threads a plugin starts itself:
// OpenMP, std::thread, Python threading), and is kept in the deprecated
// PluginManager::myPrefix for plugins compiled against older headers.  Those
// plugins read myPrefix without any lock, exactly as before (an inherent race
// with parallel kitties that only rebuilding against prefix() removes).
std::string PluginManager::myPrefix = "";   // written under latestPrefixMutex
// Never destroyed: a plugin may call exit() while other threads use it.
static std::mutex& latestPrefixMutex = *new std::mutex;
// The copy prefix() reads: never destroyed either, unlike myPrefix (a plain
// static, kept for older plugins), so a thread a plugin left running while
// the process exits still reads a live string.
static std::string& latestPrefix = *new std::string;

void PluginManager::setThreadPrefix(const std::string& prefix) {
    currentPrefix = internPrefix(prefix, true);   // (a pipeline thread)
    currentPrefixName = expectedPrefix(prefix);
}

void PluginManager::setPrefix(const std::string& prefix) {
    setThreadPrefix(prefix);
    std::lock_guard<std::mutex> lock(latestPrefixMutex);
    myPrefix = prefix;
    latestPrefix = prefix;
    ++prefixVersion;
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

// Live DependencyNotMet objects per creating thread (token: a small number
// given to each thread on first use).  Never destroyed: exceptions may be
// destroyed during exit().
static std::mutex& dependencyExceptionsMutex = *new std::mutex;
static std::map<unsigned, int>& liveDependencyExceptions = *new std::map<unsigned, int>;
static std::atomic<unsigned> nextThreadToken(1);

static unsigned threadToken() {
    static thread_local unsigned token = nextThreadToken++;
    return token;
}

PLUMA_API unsigned plumaDependencyExceptionCreated() {
    const unsigned token = threadToken();
    std::lock_guard<std::mutex> lock(dependencyExceptionsMutex);
    ++liveDependencyExceptions[token];
    return token;
}

PLUMA_API void plumaDependencyExceptionDestroyed(unsigned creator) {
    std::lock_guard<std::mutex> lock(dependencyExceptionsMutex);
    std::map<unsigned, int>::iterator it = liveDependencyExceptions.find(creator);
    if (it != liveDependencyExceptions.end() && --it->second <= 0) liveDependencyExceptions.erase(it);
}

bool PluginManager::dependencyExceptionAlive() {
    const unsigned token = threadToken();
    std::lock_guard<std::mutex> lock(dependencyExceptionsMutex);
    return liveDependencyExceptions.count(token) != 0;
}

// Constant-initialized (no constructor), so it reads false even when pluma.exe
// was mapped without running its initializers.
static bool hostProcess = false;

void PluginManager::markHostProcess() {
    hostProcess = true;
}

bool PluginManager::inHostProcess() {
    return hostProcess;
}

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

void PluginManager::dependency(std::string plugin) {
    if (getInstance().installed.count(plugin) == 0) {
        log("Plugin dependency " + plugin + " not met.");
        if (!onPipelineThread()) {
            // Also on stderr: the log file may not be open yet (e.g. called
            // from a plugin's static initializer during discovery).
            // (C stream, bounded: std::cerr is tied to std::cout, and a thread
            // blocked on a full stdout pipe holds that stream's lock.)
            const std::string message = "[PluMA] Plugin dependency " + plugin +
                                        " not met outside a pipeline step; ending the run.\n";
            pluma::runBounded([message] {
                std::fputs(message.c_str(), stderr);
                std::fflush(stderr);
            }, std::chrono::seconds(2), true);
            abortRun(false);   // includes this step's output
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

static void (*abortCleanup)(bool) = NULL;

void PluginManager::setAbortCleanup(void (*cleanup)(bool)) {
    abortCleanup = cleanup;
}

void PluginManager::abortRun(bool keepCallersOutput) {
    // Incomplete outputs first: nothing below (Python) can then skip it.
    if (abortCleanup) abortCleanup(keepCallersOutput);
    // Earlier steps' Python output still buffered in sys.stdout (redirected
    // output is block-buffered) is written before the C streams are flushed.
    // (Every language's: Python's sys.stdout, Java's System.out, ...)
    for (size_t i = 0; i < supported.size(); ++i) supported[i]->flushBeforeAbort();
    // The flush can take seconds, while steps on other kitties go on: once
    // more for what they changed meanwhile (no new step starts once the
    // cleanup has begun; one that finished meanwhile left a complete output).
    if (abortCleanup) abortCleanup(keepCallersOutput);
    // Bounded, like the flushes above: a plugin blocked in printf on a full
    // pipe holds that stream's lock, and the run must end regardless.
    // Only the console streams: the outputs' streams were flushed before the
    // cleanup, and a flush now would write into files it emptied.
    pluma::runBounded([] {
        std::cout.flush();
        std::cerr.flush();
        std::fflush(stdout);
        std::fflush(stderr);
    }, std::chrono::seconds(2), true);
    std::_Exit(1);
}

static std::atomic<int> runningKitties(0);

void PluginManager::kittyLaunching() { ++runningKitties; }
void PluginManager::kittyFinished() { --runningKitties; }
int PluginManager::kittiesRunning() { return runningKitties.load(); }

void PluginManager::installForkHandlers() {
#ifndef _WIN32
    // Hold logMutex across fork() itself, then release it on both sides: the
    // child (whose only thread is the one that forked) gets it unlocked.
    struct Handlers {
        // Also the prefix locks (prefix() in a forked multiprocessing worker
        // takes both).  None of these is ever held while taking another, so
        // any fixed order is deadlock-free.
        static void prepare() {
            logMutex().lock();
            latestPrefixMutex.lock();
            sharedInternMutex.lock();
            dependencyExceptionsMutex.lock();   // dependency() in the child
        }
        static void release() {
            dependencyExceptionsMutex.unlock();
            sharedInternMutex.unlock();
            latestPrefixMutex.unlock();
            logMutex().unlock();
        }
    };
    pthread_atfork(&Handlers::prepare, &Handlers::release, &Handlers::release);
#endif
}

static thread_local bool pipelineThread = false;

void PluginManager::setOnPipelineThread() {
    pipelineThread = true;
}

void PluginManager::releaseThreadPrefixBuffers() {
    std::lock_guard<std::mutex> lock(sharedInternMutex);
    // The map only: the buffers stay with whatever still holds them.
    delete ownPrefixBuffers;
    ownPrefixBuffers = NULL;
    currentPrefix = NULL;
    currentPrefixName = NULL;
}

void PluginManager::refreshThreadPrefix() {
    if (currentPrefix && currentPrefixName && std::strcmp(currentPrefix, currentPrefixName->c_str()) != 0)
        currentPrefix = internPrefix(*currentPrefixName, true);   // (a fresh copy: that one was written into)
}

// (Static initialization runs on the main thread.)
static const std::thread::id plumaMainThread = std::this_thread::get_id();

bool PluginManager::isMainThread() {
    return std::this_thread::get_id() == plumaMainThread;
}

void PluginManager::clearOnPipelineThread() {
    pipelineThread = false;
}

// No probing for OpenMP regions (calling into every loaded OpenMP runtime
// could initialize one that was never used -- with two runtimes loaded,
// Intel/LLVM libomp then aborts the process).  On the pipeline thread
// dependency() throws; where that exception cannot propagate -- out of an
// OpenMP parallel region, a noexcept function -- the runtime calls
// std::terminate and PluMA's terminate handler ends the run in order.
bool PluginManager::onPipelineThread() {
    return pipelineThread;
}


char* PluginManager::prefix() {
    if (!currentPrefix) {
        // A thread started (or reused, e.g. an OpenMP pool) by a plugin: the
        // most recently set prefix, as of this call.
        // Reused while the prefix has not been set again and the shared
        // buffer is intact: if another such thread wrote into it (strtok),
        // a fresh copy instead of the damaged one.
        const unsigned version = prefixVersion.load();
        if (fallbackCurrent && version == fallbackVersion && std::strcmp(fallbackCurrent, fallbackExpected->c_str()) == 0)
            return (char*) fallbackCurrent;
        if (version != fallbackVersion || !fallbackCurrent) {
            std::string latest;
            {
                std::lock_guard<std::mutex> lock(latestPrefixMutex);
                latest = latestPrefix;
            }
            fallbackExpected = expectedPrefix(latest);
        }
        fallbackCurrent = internPrefix(*fallbackExpected, false);   // (a plugin's own thread)
        fallbackVersion = version;
        return (char*) fallbackCurrent;
    }
    return (char*) currentPrefix;
}

#ifdef _WIN32
// The plugin API as plain C, exported by pluma.exe for modules built with
// another compiler -- PerlPluMA, which must be built with Strawberry Perl's
// g++ and so cannot use PluginManager's C++ interface (see
// src/bridges/PluMAHost.cxx).  Nothing here throws.
#define PLUMA_HOST_API extern "C" __declspec(dllexport)

PLUMA_HOST_API int pluma_host_api_version(void) { return 1; }

PLUMA_HOST_API int pluma_host_in_host_process(void) { return PluginManager::inHostProcess() ? 1 : 0; }

PLUMA_HOST_API void pluma_host_log(const char* message) {
    try { PluginManager::log(message ? message : ""); } catch (...) {}
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

// Valid for the rest of the run (prefixes are interned).
PLUMA_HOST_API const char* pluma_host_prefix(void) { return PluginManager::prefix(); }

PLUMA_HOST_API void pluma_host_language_load(const char* lang) {
    try { PluginManager::languageLoad(lang ? lang : ""); } catch (...) {}
}

PLUMA_HOST_API void pluma_host_language_unload(const char* lang) {
    try { PluginManager::languageUnload(lang ? lang : ""); } catch (...) {}
}

PluginManager& PluginManager::getInstance()
{
    // Never destroyed: a plugin may call exit() while parallel kitties are
    // still logging, and static destruction would delete the log file and
    // its mutex under them.  (Every log line is flushed.)
    static PluginManager* instance = new PluginManager();
    return *instance;
}
#endif
