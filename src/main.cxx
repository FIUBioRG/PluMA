/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018 Bioinformatics Research Group (BioRG)
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

#include <stdio.h>
#include <stdlib.h>
#include "Plugin.h"
#include "PluginProxy.h"
#include <string>
#include <map>
#include <deque>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <exception>
#include <cctype>
#include "platform.h"
using std::thread;
using std::vector;

// Kept from earlier versions: the pluma executable exports it (-rdynamic), and
// C++ plugins compiled against them may call it.  Not used by PluMA itself.
std::string toString(int val) {
    return std::to_string(val);
}

// Set when any plugin fails.  readConfig() may run on parallel "Kitty"
// threads, so a failure must not call exit() there (that would tear down
// statics such as the log file under the other threads); instead every
// readConfig() stops reading, joins its threads and returns, and main()
// exits once everything has unwound.
static std::atomic<bool> pipelineFailed(false);
// A step failed and the pipeline went on past it (PLUMA_CONTINUE_ON_ERROR):
// the run still ends with status 1, so scripts and CI see the failure.
static std::atomic<bool> stepFailedContinued(false);

// PLUMA_CONTINUE_ON_ERROR set to a non-empty value other than "0": a failed
// step that left no output does not stop the pipeline (earlier versions'
// behavior; see readConfig).
static bool continueOnError() {
    static const bool enabled = [] {
        const char* v = getenv("PLUMA_CONTINUE_ON_ERROR");
        return v != NULL && *v != '\0' && std::string(v) != "0";
    }();
    return enabled;
}

// The PluMA process itself (set in main()): a child a plugin fork()s inherits
// the atexit handler below and the kitty count, but has no kitty threads.
#ifndef _WIN32
static pid_t plumaPid = 0;
#endif

// The output file of every step currently running, by thread (a step's
// output is incomplete until it ends).  Never destroyed (exit()).
// With the output as it was before the step (see readConfig's outputBefore).
struct RunningStep {
    std::string output;
    pluma::FileStamp before;
    // Set by the abort cleanup once it dealt with the output, with the output
    // as it left it: its second pass acts only if the step wrote again since.
    bool handled = false;
    pluma::FileStamp left = pluma::FileStamp();
};
static std::mutex& runningOutputsMutex = *new std::mutex;
static std::map<std::thread::id, RunningStep>& runningOutputs = *new std::map<std::thread::id, RunningStep>;

// Set when the run is being aborted (removeRunningOutputs): no further step
// starts -- its output could be written after the cleanup.
static std::atomic<bool> abortingRun(false);

struct RunningOutput {   // registers this thread's step for its duration
    RunningOutput(const std::string& output, const pluma::FileStamp& before) {
        RunningStep step;   // (not an aggregate in C++11: default member initializers)
        step.output = output;
        step.before = before;
        std::unique_lock<std::mutex> lock(runningOutputsMutex);
        // A kitty reaching its next step while the run is aborted waits for
        // the process to end instead.  Checked under the lock the cleanup
        // holds while it sets the flag and looks at the running steps: a step
        // is either seen by the cleanup or never starts.
        while (abortingRun.load()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            lock.lock();
        }
        runningOutputs[std::this_thread::get_id()] = step;
    }
    ~RunningOutput() {
        std::lock_guard<std::mutex> lock(runningOutputsMutex);
        runningOutputs.erase(std::this_thread::get_id());
    }
};

// Removes a failed or cut-off step's possibly incomplete output -- only a
// regular file, never a directory, device (/dev/null) or FIFO -- logging why;
// warns if it cannot be removed (e.g. on Windows, while a plugin holds it open).
// Returns whether there was such a file to remove.
static bool removeIncompleteOutput(const std::string& output, const std::string& why) {
    if (!pluma::isRegularFile(output)) return false;
    PluginManager::log("REMOVING OUTPUT FILE: " + output + why + ".");
    if (std::remove(output.c_str()) != 0) {
        // Still open in a running step (Windows): at least leave no partial
        // content behind -- after the open stream's buffer went to the file
        // (written at exit otherwise, at its old offset, after zeros).
        pluma::flushAllStreamsBounded();
        if (pluma::truncateFile(output)) {
            PluginManager::log("Could not remove " + output + " (still open in a running step); emptied it instead.");
            pluma::warnBounded("[PluMA] Warning: could not remove incomplete output " + output +
                        " (still open); emptied it instead.");
            return true;
        }
        PluginManager::log("WARNING: could not remove " + output + "; it may be incomplete.");
        pluma::warnBounded("[PluMA] Warning: could not remove possibly incomplete output " + output);
    }
    return true;
}

// The cleanup for PluginManager::abortRun(): removes the outputs that the
// steps still running have changed -- one a step cut off on another thread
// left untouched (e.g. a valid result from an earlier run, the step not
// having written yet) is kept, as for a step that never started.  The same
// holds for the calling thread's own step when an unmet dependency()
// aborted the run (keepCallersOutput false): its output is removed only if
// it changed, as an unmet dependency that propagates normally keeps an
// untouched output.  A caller on a thread the plugin started itself
// (std::thread, OpenMP) has no step of its own here, so which step it
// belongs to is unknown: an unmet dependency() still removes every changed
// output.  exit() there may come after the caller's own step finished its
// output, while other running steps were certainly cut off -- and which
// step is the caller's cannot be told (not even with a single running step:
// the thread may be left over from an earlier step that has finished).  So
// every changed output is moved aside to "<output>.incomplete": nothing is
// left under its real name for a later step or run to take as valid, and a
// finished result is not lost.  The same for the output of a step that
// calls exit() itself (keepCallersOutput, with its step): it may have ended
// it with any status, its output finished or not, and the run ends with
// status 1; the other running steps' changed outputs are removed (they were
// cut off).
// Nothing at all in a fork()ed child: the steps are the parent's.
static void removeRunningOutputs(bool keepCallersOutput) {
#ifndef _WIN32
    if (getpid() != plumaPid) return;
#endif
    // What the steps' open C/C++ streams still buffer goes to their files
    // first: a copy aside then has it all, and an output emptied below (still
    // open, Windows) is not refilled later by a flush at its old offset.
    pluma::flushAllStreamsBounded();
    std::lock_guard<std::mutex> lock(runningOutputsMutex);
    abortingRun = true;   // (under the lock: see RunningOutput)
    const bool callerHasStep = runningOutputs.count(std::this_thread::get_id()) != 0;
    for (std::map<std::thread::id, RunningStep>::iterator it = runningOutputs.begin();
         it != runningOutputs.end(); ++it) {
        const bool callers = it->first == std::this_thread::get_id();
        RunningStep& step = it->second;
        // Nothing there (never written, or moved/removed by the first pass of
        // abortRun): nothing to move or remove.
        if (!pluma::fileExists(step.output)) continue;
        // Dealt with by the first pass, and not written since: nothing new.
        if (step.handled && pluma::fileStampLike(step.output, step.left) == step.left) continue;
        const bool changed = pluma::fileStampLike(step.output, step.before) != step.before;
        if (!changed) continue;
        if (keepCallersOutput && (callers || !callerHasStep)) {
            // A name no file has, claimed by creating it (another run setting
            // the same output aside cannot take it too): "<output>.incomplete",
            // ".incomplete.1", ... -- an earlier run's is never replaced --
            // then names with this process's id.
            bool claimed = false;
            const std::string aside = pluma::claimFreeName(
                [&](int n) { return step.output + ".incomplete" + (n ? "." + std::to_string(n) : ""); },
                [&](int n) {
                    return step.output + ".incomplete.p" + std::to_string(pluma::processId()) +
                           (n ? "-" + std::to_string(n) : "");
                },
                pluma::claimNewFile, claimed);
            if (claimed && pluma::renameReplacing(step.output, aside)) {
                PluginManager::log("MOVED POSSIBLY INCOMPLETE OUTPUT: " + step.output + " -> " + aside +
                                   (callers ? std::string(" (the step called exit() while kitties were running).")
                                            : std::string(" (run ended by exit() on a plugin's own thread while the "
                                                          "step was running).")));
                pluma::warnBounded("[PluMA] Warning: moved possibly incomplete output " + step.output + " to " + aside);
                step.handled = true;
                step.left = pluma::fileStamp(step.output);
                continue;
            }
            // Still open in a running step (Windows: no rename while open):
            // its content copied aside, then the output removed or emptied
            // below -- a finished result is kept there all the same.
            if (claimed && pluma::copyFile(step.output, aside)) {
                PluginManager::log("COPIED POSSIBLY INCOMPLETE OUTPUT: " + step.output + " -> " + aside +
                                   " (still open, so not moved).");
                pluma::warnBounded("[PluMA] Warning: copied possibly incomplete output " + step.output + " to " + aside);
            } else {
                PluginManager::log("WARNING: could not move or copy " + step.output + " aside" +
                                   (claimed ? " to " + aside : std::string(" (no free name)")) + ".");
                if (claimed) std::remove(aside.c_str());   // (the empty file claiming the name)
            }
        }
        removeIncompleteOutput(step.output, " (run aborted while the step was running)");
        step.handled = true;
        step.left = pluma::fileStamp(step.output);
    }
}

// std::terminate handler: an unmet dependency() thrown where the exception
// cannot propagate -- a noexcept function or destructor, a callback called
// from C code -- ends the run as an unmet dependency off a pipeline thread
// does (outputs cleaned up, status 1), instead of aborting.  Recognized by
// the exception itself where the runtime still has it, or else by a
// DependencyNotMet still alive on this thread (MSVC calls terminate for a
// noexcept violation without the exception being current); one a plugin
// caught and handled earlier is gone and does not count.  Anything else
// goes to the handler that was installed before (GCC/Clang's verbose
// handler prints the exception's type and what()), as without this one.
// Installed in main() and on each kitty thread (MSVC's terminate handlers
// are per thread).
static std::terminate_handler previousTerminate = nullptr;   // set in main(), before any kitty
static void onTerminate() {
    bool dependency = false;
    if (std::exception_ptr pending = std::current_exception()) {
        // The runtime still has the exception (GCC/Clang): its type decides.
        try {
            std::rethrow_exception(pending);
        } catch (const DependencyNotMet&) {
            dependency = true;
        } catch (...) {
        }
    } else {
        // It does not (MSVC, noexcept violation): a thrown DependencyNotMet
        // still alive on this thread.
        dependency = PluginManager::dependencyExceptionAlive();
    }
    if (dependency) {
        PluginManager::log("Unmet dependency where it cannot be handled (e.g. in a noexcept function); ending the run.");
        PluginManager::abortRun(false);
    }
    if (previousTerminate) previousTerminate();
    std::abort();
}

// Registered with atexit() in main().  If a plugin calls exit() while kitty
// threads are still running, exit() would go on to destroy PluMA's statics
// (the PluginManager and its maps, the language list, the prefix) under
// those threads.  Instead, end the process right here: remove the outputs
// the steps cut off on the other threads had changed (incomplete, like any
// failed step's), flush output and _Exit(1) -- the exit status the plugin
// passed to exit() is not available here, and ending a run mid-pipeline is a
// failure anyway.  The output of the step that called exit(), if it
// changed, is moved to "<output>.incomplete" (see removeRunningOutputs).  A normal end of PluMA (all kitties
// joined) is unaffected, and so is a forked child.
//
// Likewise exit() from a thread a plugin started while no kitties run: the
// main thread is still inside its step.  Not exit() on the main thread with
// no kitties (the plugin's status and outputs, as in earlier versions), nor
// a Java plugin's System.exit() then (the JVM calls exit() from a thread of
// its own; see PluginManager::markExitKeepsStatus).
static void exitWhileKittiesRun() {
#ifndef _WIN32
    if (getpid() != plumaPid) return;   // a fork()ed child: exits normally
#endif
    if (PluginManager::kittiesRunning() == 0 &&
        (PluginManager::isMainThread() || PluginManager::exitKeepsStatus()))
        return;
    PluginManager::abortRun(true);   // the exit()ing plugin's own output is moved aside, not removed
}

// Back to a pipeline's own prefix after a nested Pipeline or a LitterGather
// (input/output resolution and prefix() alike).  It is also republished as
// the global "most recent" prefix -- the fallback for threads that plugins
// start -- so those do not keep the nested pipeline's or the last kitty's
// directory.  The one exception: an empty prefix while kitties are running
// anywhere changes only this thread, since it would erase the value those
// kitties' threads may fall back to.
static void resumePipelinePrefix(const std::string& prefix) {
    if (!prefix.empty() || PluginManager::kittiesRunning() == 0) PluginManager::setPrefix(prefix);
    else PluginManager::setThreadPrefix(prefix);
}

// `pluma <pipeline> <plugin>`: restart the run at the first step running
// <plugin>.  The search spans nested Pipelines and kitties: steps are skipped
// until the restart point is reached, and everything after it runs.  A
// pipeline is handed the search only while its parent is still skipping, and
// its parent stops skipping when readConfig() reports that it (or a pipeline
// it ran) reached the point -- never because of a sibling kitty, so the
// outcome does not depend on timing.  A kitty launched while its parent
// skips runs only from the point on (nothing, if its pipeline lacks it).
// Passed down as a pointer to the point's name, which lives in main() for
// the whole run; NULL when not restarting (or no longer searching).

// The pipeline statements and how many arguments each takes -- the one
// grammar readConfig() and the restart scans share.  Any other first word
// starts a Plugin line too (as it always did, with a warning), five
// arguments: name, "inputfile", file, "outputfile", file.
struct Statement {
    const char* word;
    size_t arguments;
};
static const Statement statements[] = {
    { "Plugin", 5 }, { "Pipeline", 1 }, { "Prefix", 1 }, { "Kitty", 1 }, { "LitterLaunch", 0 }, { "LitterGather", 0 },
};
static const Statement* findStatement(const std::string& word) {
    for (const Statement& s : statements)
        if (word == s.word) return &s;
    return NULL;
}
// A Plugin line: "Plugin", or any word that is not another statement's.
static bool isPluginLine(const std::string& word) {
    const Statement* s = findStatement(word);
    return !s || std::string(s->word) == "Plugin";
}

// The pipeline files a restart scan reads (see cachedScan): set on the
// scanning thread while it scans.
static thread_local std::vector<std::string>* scannedFiles = NULL;
// This run restarts at a step (set in main() before any thread starts).
static bool restartingRun = false;

// The words of a pipeline file, for readConfig() and the restart scans
// below alike (one reader, so they always agree).  Statements are read word
// by word, so one may span several lines (as it always could).  A UTF-8 byte
// order mark is skipped; in statement position a word starting with '#'
// comments out the rest of its line.  An argument position holding what is
// evidently the next statement -- a keyword at the start of a line, a
// comment line, or a bare "#" (as in "Prefix  # set later") -- means the
// statement is cut short: the word is left for the next statement and the
// argument stays empty (an error for readConfig), instead of being swallowed
// as a file name.  On the same line, such words are ordinary arguments, as
// they always were ("inputfile #sample1.txt", "Prefix Pipeline").  So a
// file name that starts with '#' or is a statement keyword must be on the
// same line as the word before it; at the start of a continuation line it
// ends the statement ("Incomplete pipeline line").
class PipelineWords {
public:
    // The file is read whole, once: FIFOs and /dev/stdin -- `pluma
    // <(make_pipeline)` -- cannot seek back over a byte order mark.  (A
    // directory opens as a stream on POSIX and reads as empty: it does not
    // count as open.)
    // A FIFO or /dev/stdin can be read only once, but a restart scans the
    // pipeline first (scanLitterBlocks): when restarting, what such a file
    // held is kept for the run.  (Regular files are read afresh: a plugin may
    // rewrite one.  Without a restart a FIFO is opened at each use: its
    // writer may send something else each time.)
    explicit PipelineWords(const std::string& file) {
        if (scannedFiles) scannedFiles->push_back(file);
        static std::mutex& streamedMutex = *new std::mutex;   // never destroyed (exit())
        static std::map<std::string, std::string>& streamed = *new std::map<std::string, std::string>;
        const bool regularFile = pluma::isRegularFile(file);
        const bool keepContent = restartingRun && !regularFile;
        const std::string key = keepContent ? pluma::absolutePath(file, pluma::currentDirectory()) : std::string();
        if (keepContent) {
            std::lock_guard<std::mutex> lock(streamedMutex);
            std::map<std::string, std::string>::const_iterator kept = streamed.find(key);
            if (kept != streamed.end()) {
                opened = true;
                in.str(kept->second);
                return;
            }
        }
        std::ifstream f(file.c_str(), std::ios::in | std::ios::binary);
        opened = f.is_open() && !pluma::isDirectory(file);
        if (!opened) return;
        std::string data;
        failed = !pluma::readWholeStream(f, data);
        pluma::stripUtf8Bom(data);   // UTF-8 byte order mark
        if (keepContent && !failed) {
            std::lock_guard<std::mutex> lock(streamedMutex);
            streamed[key] = data;
        }
        in.str(data);
    }
    // The next statement word (comments skipped).
    bool next(std::string& word) {
        while (nextWord(word)) {
            if (word[0] != '#') return true;
            std::string rest;
            std::getline(in, rest);
            lineStartPending = true;
        }
        return false;
    }
    bool argument(std::string& arg) {
        if (!nextWord(arg)) { arg.clear(); return false; }
        const bool nextStatement = atLineStart && (isKeyword(arg) || arg[0] == '#');
        if (nextStatement || arg == "#") {
            pending = arg; pendingAtLineStart = atLineStart; havePending = true;
            arg.clear();
            return false;
        }
        return true;
    }
    bool isOpen() const { return opened; }
    // A read error (not the end of the file) stopped reading.
    bool readFailed() const { return failed; }
private:
    bool nextWord(std::string& w) {
        if (havePending) { w = pending; atLineStart = pendingAtLineStart; havePending = false; return true; }
        bool newline = lineStartPending;
        lineStartPending = false;
        int c;
        while ((c = in.get()) != EOF && std::isspace(static_cast<unsigned char>(c)))
            if (c == '\n') newline = true;
        if (c == EOF) return false;
        w.assign(1, static_cast<char>(c));
        while ((c = in.peek()) != EOF && !std::isspace(static_cast<unsigned char>(c)))
            w.push_back(static_cast<char>(in.get()));
        atLineStart = newline;
        return true;
    }
    static bool isKeyword(const std::string& w) { return findStatement(w) != NULL; }
    std::istringstream in;
    bool opened = false, failed = false;
    std::string pending;
    bool havePending = false, pendingAtLineStart = false;
    bool lineStartPending = true, atLineStart = false;
};

// One statement's arguments, read for readConfig() and the restart scans
// alike (see `statements`): `word` is its first word.  Reading stops at the
// first missing argument, which stays empty along with the rest.
static void readStatement(PipelineWords& words, const std::string& word, std::vector<std::string>& args) {
    const Statement* statement = findStatement(word);
    const size_t count = statement ? statement->arguments : 5;   // (a stray word: a Plugin line)
    args.assign(count, std::string());
    for (size_t i = 0; i < count && words.argument(args[i]); ++i) {}
}

// Whether running `file` would reach a step `point`: a Plugin line naming it,
// in the file or in a Pipeline it runs (nested, kitties included).  A static
// scan, so whether a kitty holds the restart point is known when it is
// launched -- independent of how far its siblings have run.  `seen` caches
// each file's answer for one scan; a file being scanned (a Pipeline that
// includes itself, directly or not) counts as not holding it, so every file
// is read at most once.
static bool pipelineContainsStep(const std::string& file, const std::string& point,
                                 std::map<std::string, int>& seen) {
    std::map<std::string, int>::const_iterator known = seen.find(file);
    if (known != seen.end()) return known->second == 1;   // 0: being scanned
    seen[file] = 0;
    PipelineWords words(file);
    std::string word;
    std::vector<std::string> args;
    bool found = false;
    while (!found && words.next(word)) {
        readStatement(words, word, args);
        const std::string arg = args.empty() ? std::string() : args[0];
        if (word == "Pipeline") found = !arg.empty() && pipelineContainsStep(arg, point, seen);
        else if (isPluginLine(word)) found = arg == point;
    }
    seen[file] = found ? 1 : 2;
    return found;
}
// A restart scan's answer for a whole pipeline file, remembered for the run:
// a block of many kitties running the same pipeline reads its files once,
// not once per kitty.  (Only these top-level answers: one computed inside a
// scan may be partial -- a file being scanned counts as not holding the
// point.)  Kitty threads launch kitties too: guarded.  By absolute path: a
// relative one follows the current directory, which a plugin may change.
template <class Result>
static Result cachedScan(const std::string& file, const std::string& point,
                         Result (*scan)(const std::string&, const std::string&)) {
    // The answer, with the files the scan read and how each looked then: a
    // plugin may rewrite a (generated) pipeline file during the run, and an
    // answer is reused only while none of them changed.
    struct Entry {
        Result result;
        std::vector<std::pair<std::string, pluma::FileStamp> > files;
    };
    static std::mutex& cacheMutex = *new std::mutex;   // never destroyed (exit()); one per Result type
    static std::map<std::pair<std::string, std::string>, Entry>& cache =
        *new std::map<std::pair<std::string, std::string>, Entry>;
    const std::pair<std::string, std::string> key(pluma::absolutePath(file, pluma::currentDirectory()), point);
    // The entry is copied under the lock and its files checked outside it, so
    // one thread's stat calls do not hold up the others.
    Entry cached;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        typename std::map<std::pair<std::string, std::string>, Entry>::const_iterator hit = cache.find(key);
        if (hit != cache.end()) {
            cached = hit->second;
            found = true;
        }
    }
    if (found) {
        bool unchanged = true;
        for (size_t i = 0; unchanged && i < cached.files.size(); ++i)
            unchanged = pluma::fileStampLike(cached.files[i].first, cached.files[i].second) == cached.files[i].second;
        if (unchanged) return cached.result;
    }
    std::vector<std::string> read;
    std::vector<std::string>* outer = scannedFiles;   // (scans do not nest; restored all the same)
    scannedFiles = &read;
    Entry entry;
    try {
        entry.result = scan(file, point);
    } catch (...) {
        scannedFiles = outer;
        throw;
    }
    scannedFiles = outer;
    for (size_t i = 0; i < read.size(); ++i)
        entry.files.push_back(std::make_pair(pluma::absolutePath(read[i], pluma::currentDirectory()),
                                             // (Hashed if just changed, and then compared by
                                             // content: a rewrite within the same timestamp
                                             // tick at the same size still counts.)
                                             pluma::fileStamp(read[i])));
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache[key] = entry;
    return entry.result;
}
static bool scanPipelineForStep(const std::string& file, const std::string& point) {
    std::map<std::string, int> seen;
    return pipelineContainsStep(file, point, seen);
}
static bool pipelineContainsStep(const std::string& file, const std::string& point) {
    return cachedScan<bool>(file, point, scanPipelineForStep);
}

// For each LitterLaunch block of `file` (in order): whether the restart point
// is in one of its kitties, and whether it is on the block's own thread (a
// Plugin line, or a sequential Pipeline, inside the block).  Kitties of one
// block run side by side, so a block holding the point is not "before" it:
// its other kitties run in full (see readConfig).
struct LitterBlock {
    bool inKitty = false;
    bool onMain = false;
};
static std::vector<LitterBlock> scanLitterBlocksOf(const std::string& file, const std::string& point) {
    std::vector<LitterBlock> blocks;
    std::map<std::string, int> seen;
    seen[file] = 0;   // this file is being scanned (see pipelineContainsStep)
    PipelineWords words(file);
    std::string word;
    std::vector<std::string> args;
    bool inBlock = false, kittySeen = false;
    while (words.next(word)) {
        readStatement(words, word, args);
        const std::string arg = args.empty() ? std::string() : args[0];
        // The kitty state as readConfig() keeps it: a Kitty line (even one
        // before LitterLaunch) makes later Pipelines kitties until the
        // LitterGather.
        if (word == "LitterLaunch") {
            blocks.push_back(LitterBlock());
            inBlock = true;
        } else if (word == "LitterGather") {
            inBlock = false;
            kittySeen = false;
        } else if (word == "Kitty") {
            if (!arg.empty()) kittySeen = true;
        } else if (word == "Pipeline") {
            if (!arg.empty() && inBlock && pipelineContainsStep(arg, point, seen)) {
                if (kittySeen) blocks.back().inKitty = true;
                else blocks.back().onMain = true;
            }
        } else if (isPluginLine(word)) {
            if (inBlock && arg == point) blocks.back().onMain = true;
        }
    }
    return blocks;
}
// The same, remembered for the run (see cachedScan): the kitties of a
// fan-out each scan the same pipeline when restarting.
static std::vector<LitterBlock> scanLitterBlocks(const std::string& file, const std::string& point) {
    return cachedScan<std::vector<LitterBlock> >(file, point, scanLitterBlocksOf);
}

// Returns whether the restart point was reached in this pipeline or one it
// ran (false when not searching).
bool readConfig(std::string inputfile, std::string prefix, const std::string* restart);

// Joins the kitties launched so far -- at LitterGather, and at the end of a
// pipeline for any never gathered (a joinable std::thread must not be
// destroyed) -- and folds whether one of them reached the restart point into
// `skipping`.  `announce`: print "Gathering Kitty i" for each, as
// LitterGather always did.
static void gatherKitties(std::vector<std::thread>& threads, std::deque<char>& reachedRestart, bool& skipping,
                          bool announce) {
    for (size_t i = 0; i < threads.size(); i++) {
        if (announce) std::cout << "[PluMA] Gathering Kitty " << i << std::endl;
        if (threads[i].joinable()) threads[i].join();
    }
    for (size_t i = 0; i < reachedRestart.size(); i++)
        if (reachedRestart[i]) skipping = false;   // reached in one of them
    threads.clear();
    reachedRestart.clear();
}

// readConfig() as the top of a thread: anything thrown outside a step (e.g.
// out of memory reading a pipeline) stops the run in order, as a failed step
// does, instead of ending in std::terminate.
static bool readConfigGuarded(const std::string& pipeline, const std::string& prefix, const std::string* restart) {
    try {
        return readConfig(pipeline, prefix, restart);
    } catch (const std::exception& e) {
        PluginManager::getInstance().log("ERROR reading pipeline " + pipeline + ": " + e.what());
        std::cout << "[PluMA] Error reading pipeline " << pipeline << ": " << e.what() << ". Exiting." << std::endl;
    } catch (...) {
        PluginManager::getInstance().log("ERROR reading pipeline " + pipeline + ".");
        std::cout << "[PluMA] Error reading pipeline " << pipeline << ". Exiting." << std::endl;
    }
    pipelineFailed = true;
    return false;
}

// Body of a parallel kitty thread.  The launching thread counts it in
// PluginManager::kittiesRunning() *before* creating the thread (so resumePipelinePrefix()
// never sees 0 for a kitty that is about to start); the kitty uncounts
// itself when it ends.
static void runKitty(std::string pipeline, std::string prefix, const std::string* restart, char* reachedRestart) {
    struct Uncount {
        ~Uncount() {
            PluginManager::releaseThreadPrefixBuffers();
            PluginManager::kittyFinished();
        }
    } uncount;
    std::set_terminate(onTerminate);   // per thread with MSVC (see onTerminate)
    *reachedRestart = readConfigGuarded(pipeline, prefix, restart) ? 1 : 0;   // read after join()
}


bool readConfig(std::string inputfile, std::string prefix, const std::string* restart) {
    PluginManager::setOnPipelineThread();   // plugin exceptions here are handled below
    PipelineWords words(inputfile);   // (a UTF-8 byte order mark is skipped)
    if (!words.isOpen()) {
        PluginManager::getInstance().log("ERROR: cannot open pipeline file "+inputfile+".");
        std::cout << "[PluMA] Cannot open pipeline file " << inputfile << ". Exiting." << std::endl;
        pipelineFailed = true;
        return false;
    }
    // Skipping steps until the restart point (see the restart point's comment above).
    // A pipeline is handed the search only while its parent is still
    // skipping (so whether a kitty skips never depends on its siblings).
    bool skipping = restart != NULL;
    // The plugin-visible prefix is per thread: a kitty thread starts with the
    // prefix its parent passed in (see PluginManager::setPrefix()).
    if (!prefix.empty()) PluginManager::setPrefix(prefix);
    std::string pipeline = "";
    std::string oldprefix = prefix;
    std::string kitty = "";
    bool parallelflag = false, kittyflag = false;
    vector<std::thread> threads;
    // Per launched kitty: whether it reached the restart point (written by
    // the kitty, read after join(); a deque keeps the addresses stable).
    std::deque<char> kittyReachedRestart;
    // Something thrown outside a step (see readConfigGuarded) while kitties
    // still run: no new steps, and they are joined -- destroying a joinable
    // std::thread would end the process in std::terminate.
    struct JoinKitties {
        std::vector<std::thread>& threads;
        std::deque<char>& reachedRestart;
        ~JoinKitties() {
            if (threads.empty()) return;   // (gathered: the normal end)
            pipelineFailed = true;
            bool ignored = false;
            gatherKitties(threads, reachedRestart, ignored, false);
        }
    } joinKitties = { threads, kittyReachedRestart };
    // Restarting: this file's LitterLaunch blocks (see scanLitterBlocks).
    // In a block that holds the point, the kitty holding it runs from it and
    // its sibling kitties run in full (`blockHoldsPoint`).
    std::vector<LitterBlock> litterBlocks;
    if (restart) litterBlocks = scanLitterBlocks(inputfile, *restart);
    size_t litterIndex = 0;
    bool blockHoldsPoint = false;
    LitterBlock currentBlock;
    // A statement missing its argument stops the pipeline: running it anyway
    // would use a stale or empty value (a bare "Prefix" meant "/", a bare
    // "Kitty" reused the previous kitty's directory, a bare "Pipeline" re-ran
    // the previous pipeline).
    auto incompleteLine = [&](const std::string& what, const std::string& expected) {
        PluginManager::getInstance().log("ERROR: incomplete line for "+what+" in "+inputfile+
                                         " (expected: "+expected+").");
        std::cout << "[PluMA] Incomplete pipeline line for " << what << " in " << inputfile
                  << ". Exiting." << std::endl;
        pipelineFailed = true;
    };
    // Statements and their arguments: see PipelineWords.
    std::vector<std::string> args;   // the statement's arguments (see readStatement)
    std::string junk;
    // Testing the read itself (not eof()) also ends the loop on a read error.
    while (!pipelineFailed && words.next(junk)) {   // (comment lines skipped)
        readStatement(words, junk, args);
        std::string name, inputname, outputname;
        /**
        * Read one line
        * Plugin (Name) inputfile (input file) outputfile (output file)
        */
        if (junk == "Prefix") {
            // the line is a prefix
            std::string value = args[0];
            if (value.empty()) { incompleteLine("Prefix", "Prefix <directory>"); break; }
            // One separator after it, however the line ended ("out/" -> "out/",
            // not "out//"); "/" stays the root.
#ifdef _WIN32
            const std::string separators = "/\\";
#else
            const std::string separators = "/";   // (a backslash is an ordinary character in a POSIX name)
#endif
            while (value.size() > 1 && separators.find(value.back()) != std::string::npos) value.pop_back();
            prefix = (value.size() == 1 && separators.find(value[0]) != std::string::npos) ? value : value + "/";
            PluginManager::setPrefix(prefix);
            oldprefix = prefix;
            continue;
        } else if (junk == "Pipeline") {
            pipeline = args[0];
            if (pipeline.empty()) { incompleteLine("Pipeline", "Pipeline <file>"); break; }
	    if (parallelflag && kittyflag) { // Kitty is multithreaded
		PluginManager::kittyLaunching();   // uncounted by runKitty, or below if no thread starts
		try {
		    // The slot first, so nothing can throw once the thread runs
		    // (a joinable std::thread destroyed during unwinding terminates).
		    threads.emplace_back();
		    kittyReachedRestart.push_back(0);
		    // Restarting: in a block that holds the restart point, every kitty
		    // whose pipeline holds it runs from the point (a fan-out of the same
		    // pipeline resumes alike in each kitty) and every other sibling
		    // runs in full (they run side by side with it, not before it -- and
		    // a sibling may have been cut off when the run stopped).  The
		    // kitties of a block before the point are skipped, like any
		    // earlier step.
		    const std::string* kittyRestart = skipping ? restart : NULL;
		    if (blockHoldsPoint) {
		        // With the point on the block's own thread too, this thread
		        // skips until it gets there; kitties launched after that ran
		        // run in full, as any step after the point.
		        const bool pointAhead = skipping || !currentBlock.onMain;
		        kittyRestart = pointAhead && currentBlock.inKitty && pipelineContainsStep(pipeline, *restart)
		                           ? restart : NULL;
		    }
		    threads.back() = std::thread(runKitty, pipeline, prefix, kittyRestart,
		                                 &kittyReachedRestart.back());
		} catch (const std::exception& e) {   // std::system_error: no more threads; bad_alloc
		    if (!threads.empty() && !threads.back().joinable()) threads.pop_back();
		    // Its restart slot too, so both lists stay one entry per kitty.
		    while (kittyReachedRestart.size() > threads.size()) kittyReachedRestart.pop_back();
		    PluginManager::kittyFinished();
		    // An orderly stop: the kitties already launched are joined below
		    // (rethrowing would destroy their joinable threads: std::terminate).
		    PluginManager::getInstance().log(std::string("ERROR: could not start kitty ") + kitty + ": " + e.what());
		    std::cout << "[PluMA] Could not start kitty " << kitty << ". Exiting." << std::endl;
		    pipelineFailed = true;
		    break;
		}
	    }
	    else {
            if (readConfig(pipeline, prefix, skipping ? restart : NULL)) skipping = false;   // reached in it
            // The nested pipeline may have set its own Prefix on this thread;
            // restore ours for the plugins that follow.
            resumePipelinePrefix(prefix);
	    }
            continue;   // control line: no plugin to run
        } else if (junk == "Kitty") {
            kitty = args[0];
            if (kitty.empty()) { incompleteLine("Kitty", "Kitty <name>"); break; }
            // Each kitty's directory under the pipeline's Prefix.  Without a
            // Prefix, a relative "<kitty>/" -- not "/<kitty>/" at the
            // filesystem root, accumulating across kitties as it used to.
            if (oldprefix != "") {
                prefix = oldprefix + kitty + "/";   // (a Prefix ends in its one separator)
            } else {
                prefix = kitty + "/";
            }
            PluginManager::setPrefix(prefix);
	    kittyflag = true;
            continue;
	}
	else if (junk == "LitterLaunch") {
            parallelflag = true;
            currentBlock = litterIndex < litterBlocks.size() ? litterBlocks[litterIndex] : LitterBlock();
            ++litterIndex;
            blockHoldsPoint = skipping && (currentBlock.inKitty || currentBlock.onMain);
            // The point is in one of this block's kitties: the steps of the
            // block on this thread run side by side with it, not before it.
            if (blockHoldsPoint && currentBlock.inKitty && !currentBlock.onMain) skipping = false;
            continue;   // control line: no plugin to run
	}
	else if (junk == "LitterGather") {
	    gatherKitties(threads, kittyReachedRestart, skipping, true);
	    blockHoldsPoint = false;
	    parallelflag = false;
	    kittyflag = false;
            // Kitty lines redirected this pipeline's prefix into each kitty's
            // directory; after the gather, plugins run under the pipeline's
            // own Prefix again (input/output resolution and prefix() alike).
            prefix = oldprefix;
            resumePipelinePrefix(prefix);
            continue;   // control line: no plugin to run
        }
        const std::string keyword = junk;   // "Plugin" (or a stray word)
        // Plugin <name> inputfile <file> outputfile <file>; stops at the first
        // missing word, leaving the rest empty.
        std::string inKeyword, outKeyword;
        name = args[0]; inKeyword = args[1]; inputname = args[2]; outKeyword = args[3]; outputname = args[4];

        ////////////////////////////////////////////////////////////////////////////
        // If we are restarting and have not hit that point yet, skip this plugin
        if (skipping) {
            // Only this pipeline's own step ends its skipping here (a sibling
            // kitty finding the point must not start this one mid-way).
            if (name == *restart) {
                skipping = false;
            } else {
                continue;
            }
        }
        /////////////////////////////////////////////////////////////////////

        // Checked only for steps that will run (not ones a restart skips).
        if (name.empty()) {
            // A bare "Plugin" (or a lone unknown word): a step that cannot be
            // run is an error like any other incomplete line, never skipped.
            incompleteLine("'" + keyword + "'", "Plugin <name> inputfile <file> outputfile <file>");
            break;
        }
        if (inputname.empty() || outputname.empty()) {
            // e.g. "Plugin Foo inputfile in.txt" with no outputfile: running
            // it would hand the plugin the bare Prefix directory as a path.
            incompleteLine("plugin " + name, "Plugin <name> inputfile <file> outputfile <file>");
            break;
        }
        // The keywords: earlier versions read them without looking, so other
        // words still work (with a warning) -- except the exact swap
        // ("... outputfile a inputfile b"), which would run the plugin with
        // its files exchanged, and a failure would then remove what is
        // really its input: that stops the run.  Any letter case.
        {
            const std::string kw = pluma::toLowerAscii(keyword);
            const std::string in = pluma::toLowerAscii(inKeyword);
            const std::string out = pluma::toLowerAscii(outKeyword);
            const std::string line = keyword + " " + name + " " + inKeyword + " " + inputname + " " + outKeyword +
                                     " " + outputname;
            const std::string expected = " (expected: Plugin <name> inputfile <file> outputfile <file>)";
            if (in == "outputfile" && out == "inputfile") {   // exactly swapped; other words only warn
                const std::string msg = "Pipeline line '" + line + "' in " + inputfile +
                                        " names its output before its input" + expected + ".";
                PluginManager::getInstance().log("ERROR: " + msg);
                std::cout << "[PluMA] " << msg << " Exiting." << std::endl;
                pipelineFailed = true;
                break;
            }
            if (kw != "plugin" || in != "inputfile" || out != "outputfile") {
                const std::string msg = "Pipeline line '" + line + "' in " + inputfile +
                                        " uses unexpected keywords; running it as before" + expected + ".";
                PluginManager::getInstance().log("WARNING: " + msg);
                std::cout << "[PluMA] Warning: " << msg << std::endl;
            }
        }

        if (pluma::isPlainRelativePath(inputname)) {
            inputname = prefix + inputname;
        }

        if (pluma::isPlainRelativePath(outputname)) {
            outputname = prefix + outputname;
        }
        //////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Try to create and run all three steps of the plugin in the appropriate language
        PluginManager::getInstance().log("Creating plugin "+name);
        bool failed = false;
        bool pluginStarted = true;
        PluginManager::takeDependencyFailed();   // clear the mark for this step
        const unsigned dependencyFailuresBefore = PluginManager::dependencyFailureCount();
        PluginManager::refreshThreadPrefix();   // (an earlier step may have written into it)
        // The output as it was before the step, so a "not started" failure
        // (e.g. an unmet dependency() in input()) that nevertheless touched
        // it -- truncated or partly wrote it -- does not leave that behind.
        // The output's absolute path as of the step's start: a plugin may
        // change the current directory (os.chdir, setwd) before failing, and
        // the checks and removal below must still mean the file it was given.
        // (The plugin itself gets `outputname`, as always.)
        const std::string outputPath = pluma::absolutePath(outputname, pluma::currentDirectory());
        const pluma::FileStamp outputBefore = pluma::fileStamp(outputPath);
        try {
            bool executed = false;
            // find(), not operator[]: readConfig() runs on parallel kitty
            // threads, and operator[] would insert into the shared map.
            const std::map<std::string, std::string>& langs = PluginManager::getInstance().pluginLanguages;
            std::map<std::string, std::string>::const_iterator found = langs.find(name+"Plugin");
            for (int i = 0; i < PluginManager::supported.size() && !executed && found != langs.end(); i++) {
                if (found->second == PluginManager::supported[i]->lang()) {
                    std::cout << "[PluMA] Running Plugin: " << name << std::endl;
                    RunningOutput running(outputPath, outputBefore);   // see removeRunningOutputs
                    PluginManager::supported[i]->executePlugin(name, inputname, outputname);
                    executed = true;
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
            // The plugin is not installed (or misspelled), or its language is not supported.
            if (!executed) {
                PluginManager::getInstance().log("Error, no suitable language for plugin: "+name+".");
                throw PluginNotStarted("plugin " + name + " not found");
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // The step failed before any plugin code ran (not found, language not
        // enabled, runtime unavailable): it cannot have written its output.
        catch (const PluginNotStarted&) {
            failed = true;
            pluginStarted = false;
        }
        // The plugin errored while running.
        catch (...) {
            failed = true;
        }
        // The plugin caught its own unmet dependency() (catch (...), Python's
        // `except Exception`) and went on: it still fails, as an unmet
        // dependency always ended the run (earlier versions called exit(1)
        // inside dependency()).  Like a step that did not start, it keeps an
        // output it left unchanged.
        if (!failed && PluginManager::takeDependencyFailed()) {
            PluginManager::getInstance().log("Plugin " + name + " went on after an unmet dependency(); "
                                             "treating the step as failed.");
            failed = true;
            pluginStarted = false;
        }
        // Message(s) will be output to the logfile, and output files will be removed.
        if (failed) {
            PluginManager::getInstance().log("ERROR IN PLUGIN: "+name+".");;
            // A plugin that ran and failed may have left partial output: remove
            // it.  A step that never started keeps an existing file (e.g. a
            // valid result from an earlier run) -- unless the file changed
            // during the step after all.  Never remove a directory.
            const bool outputChanged = pluma::fileStampLike(outputPath, outputBefore) != outputBefore;
            // (removeIncompleteOutput removes only a regular file.)
            const bool removedOutput = (pluginStarted || outputChanged) && removeIncompleteOutput(outputPath, "");
            // PLUMA_CONTINUE_ON_ERROR=1: as in earlier versions, a failure
            // that left no output to remove is logged and the pipeline goes
            // on (one whose output had to be removed still stops, and so does
            // an unmet dependency(), which always ended the run) -- but the
            // run then ends with status 1, not 0.
            const bool unmetDependency = PluginManager::dependencyFailureCount() != dependencyFailuresBefore;
            if (continueOnError() && !removedOutput && !unmetDependency) {
                std::cout << "[PluMA] Error in plugin " << name << "; see the log file. Continuing "
                          << "(PLUMA_CONTINUE_ON_ERROR)." << std::endl;
                stepFailedContinued = true;
                continue;
            }
            // Later plugins may depend on this one's output; stop the pipeline.
            std::cout << "[PluMA] Error in plugin " << name << "; see the log file. Exiting." << std::endl;
            pipelineFailed = true;
            break;
        }
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
    }
    // Reading stopped by an error, not the end of the file: the rest of the
    // pipeline did not run -- a failure, not a successful run.
    if (!pipelineFailed && words.readFailed()) {
        PluginManager::getInstance().log("ERROR: reading pipeline file "+inputfile+" failed.");
        std::cout << "[PluMA] Reading pipeline file " << inputfile << " failed. Exiting." << std::endl;
        pipelineFailed = true;
    }
    // Never leave a joinable std::thread behind, e.g. after a failure or a
    // missing LitterGather; such kitties also count for whether the restart
    // point was reached (for the steps after this pipeline in its parent).
    gatherKitties(threads, kittyReachedRestart, skipping, false);
    return restart != NULL && !skipping;
}


int main(int argc, char** argv)
{
    PluginManager::markHostProcess();
#ifndef _WIN32
    plumaPid = getpid();
#endif
    PluginManager::getInstance();   // constructed before the exit guard (below)
    PluginManager::installForkHandlers();
    PluginManager::setAbortCleanup(removeRunningOutputs);
    {
        std::terminate_handler previous = std::set_terminate(onTerminate);
        if (previous != onTerminate) previousTerminate = previous;
    }
#ifdef _WIN32
    // Never resolve DLLs from the current directory, which is typically a
    // pipeline/data directory (DLL planting).  Plugins load their own
    // dependencies from their own directory (see pluma::loadLibrary).
    SetDllDirectoryA("");
#endif
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Display opening screen.
    std::cout << "***********************************************************************************" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*  PPPPPPPPPPPP    L               U           U   M           M         A        *" << std::endl;
    std::cout << "*  P           P   L               U           U   MM         MM        A A       *" << std::endl;
    std::cout << "*  P           P   L               U           U   M M       M M       A   A      *" << std::endl;
    std::cout << "*  P           P   L               U           U   M  M     M  M      A     A     *" << std::endl;
    std::cout << "*  P           P   L               U           U   M   M   M   M     A       A    *" << std::endl;
    std::cout << "*  PPPPPPPPPPPP    L               U           U   M    M M    M    A         A   *" << std::endl;
    std::cout << "*  P               L               U           U   M     M     M   AAAAAAAAAAAAA  *" << std::endl;
    std::cout << "*  P               L               U           U   M           M   A           A  *" << std::endl;
    std::cout << "*  P               L               U           U   M           M   A           A  *" << std::endl;
    std::cout << "*  P               LLLLLLLLLLLLL    UUUUUUUUUUU    M           M   A           A  *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*            [Plu]gin-based [M]icrobiome [A]nalysis (formerly MiAMi)              *" << std::endl;
    std::cout << "* (C) 2016, 2018 Bioinformatics Research Group, Florida International University  *" << std::endl;
    std::cout << "*    Under MIT License From Open Source Initiative (OSI), All Rights Reserved.    *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "*    Any professionally published work using PluMA should cite the following:     *" << std::endl;
    std::cout << "*                        T. Cickovski and G. Narasimhan.                          *" << std::endl;
    std::cout << "*                Constructing Lightweight and Flexible Pipelines                  *" << std::endl;
    std::cout << "*                 Using Plugin-Based Microbiome Analysis (PluMA)                  *" << std::endl;
    std::cout << "*                     Bioinformatics 34(17):2881-2888, 2018                       *" << std::endl;
    std::cout << "*                                                                                 *" << std::endl;
    std::cout << "***********************************************************************************" << std::endl;
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Setup plugin path.
    // PLUMA_PLUGIN_PATH's entries are separated by pluma::PATH_LIST_SEP (':' on
    // POSIX, ';' on Windows); the path built here by pluma::PLUGIN_PATH_SEP.
    // ./plugins first, then PLUMA_PLUGIN_PATH, as always (precedence rules in
    // Language::loadPlugin).
    const std::string cwd = pluma::currentDirectory();
    std::string pluginpath = cwd + "/plugins/";
    if (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        // Relative entries are made absolute now: plugins are looked up again
        // at every step, and a plugin may have changed directory by then.
        std::vector<std::string> entries = pluma::splitPathList(pluma::normalizeSlashes(getenv("PLUMA_PLUGIN_PATH")));
        for (size_t i = 0; i < entries.size(); i++) {
            pluginpath += pluma::PLUGIN_PATH_SEP;
            pluginpath += pluma::absolutePath(entries[i], cwd);
        }
    }
    pluginpath += pluma::PLUGIN_PATH_SEP;
    ///////////////////////////////////////////////////////////////////////////////////////////////


    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Command line arguments
    if (argc != 2 && argc != 3 || std::string(argv[1]) == "usage") { // Usage
        std::cout << "[PluMA] Usage: ./pluma (config file) (optional restart point)" << std::endl;
        std::cout << "Arguments: help: display this message" << std::endl;
        std::cout << "           version: display release information" << std::endl;
        std::cout << "           plugins: list your installed plugins and location" << std::endl;
        exit(0);
    } else if (std::string(argv[1]) == "help") { // Help
        std::cout << "[PluMA] Usage: ./pluma (config file) (optional restart point)" << std::endl;
        exit(0);
    } else if (std::string(argv[1]) == "version") { // Version
        std::cout << "[PluMA] Version 2.0" << std::endl;
        exit(0);
    }

    PluginManager::supportedLanguages(pluginpath, argc, argv);
    //////////////////////////////////////////////////////////////////////////////////////////
    // For each PluginManager::supported language, load the appropriate plugins
    // Each plugin directory once, in order (PLUMA_PLUGIN_PATH may repeat the
    // default ./plugins): otherwise every plugin would be reported as a
    // duplicate of itself.
    std::vector<std::string> pluginDirs;
    {
        std::vector<std::string> entries = pluma::splitPluginPath(pluginpath);
        std::set<std::string> seen;
        for (size_t i = 0; i < entries.size(); i++) {
            const std::string key = pluma::directoryKey(entries[i]);
            if (seen.insert(key).second) pluginDirs.push_back(entries[i]);
        }
    }
    bool list = false;
    for (size_t d = 0; d < pluginDirs.size(); d++) {
        const std::string& path = pluginDirs[d];
        if (std::string(argv[1]) == "plugins") {
            std::cout << "[PluMA] Plugins in " << path << ":" << std::endl;
            list = true;
        }

        // In registration order (C, Python, R, Perl, Java); precedence rules
        // in Language::loadPlugin.
        for (size_t i = 0; i < PluginManager::supported.size(); i++)
            PluginManager::supported[i]->loadPlugin(path, &(PluginManager::getInstance().pluginLanguages), list);
        // Installed for dependency() after each directory, as in earlier
        // versions: a plugin library in a later directory may check, while it
        // is loaded (a static initializer), for one found in an earlier one.
        // Only plugins that can actually run count: plugins of a language
        // compiled out of this build are registered just to report "not
        // enabled".
        std::map<std::string, std::string>::iterator itr;
        for (itr = PluginManager::getInstance().pluginLanguages.begin(); itr != PluginManager::getInstance().pluginLanguages.end(); itr++) {
            if (PluginManager::languageEnabled(itr->second))
                PluginManager::getInstance().add(itr->first.substr(0, itr->first.length()-6));
        }
    }
    // A plugin library may have started Python while it was loaded (a static
    // initializer calling Py_Initialize), leaving this thread holding the GIL.
    Py::releaseForeignGil();
    // Registered only now, after the C/C++ plugin libraries are loaded:
    // exit() runs atexit handlers and static destructors in reverse order of
    // registration, so this guard runs before the destructors of PluMA's
    // statics *and* of the plugins' own (their PluginProxy objects, globals).
    // (Objects first created later -- a function-local static first used in
    // a plugin step -- are still destroyed before it: a mitigation, not a
    // guarantee; plugins should not call exit().)
    std::atexit(exitWhileKittiesRun);
    if (list) exit(0);
    //////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////
    // Check for a restart point
    const std::string restartPoint = argc == 3 ? std::string(argv[2]) : std::string();
    const std::string* const restart = argc == 3 ? &restartPoint : NULL;   // NULL: not restarting
    restartingRun = restart != NULL;
    //////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get the current time, and setup the initial log file
    time_t t = time(0);
    struct tm* now = localtime( &t );
    const std::string tsep(1, pluma::TIMESTAMP_SEP); // ':' is not valid in Windows file names
    std::string currentTime = std::to_string(now->tm_year + 1900) + "-" + std::to_string(now->tm_mon + 1) + "-" +
                              std::to_string(now->tm_mday) + "@" + std::to_string(now->tm_hour) + tsep +
                              std::to_string(now->tm_min) + tsep + std::to_string(now->tm_sec);
    // Errors point the user at the log file: make sure it can be written.
    pluma::makeDirectories("logs");
    // A run started in the same second as another one (earlier, or at the
    // same moment) must not share its log: the name is claimed by creating
    // the file exclusively -- with a counter added while it is taken, then
    // this process's id (never another live run's log, which would be
    // truncated).  A name that cannot be created at all (an unwritable logs
    // directory) ends the search: setLogFile's failure is warned below.
    bool claimed = false;
    const std::string mylog = pluma::claimFreeName(
        [&](int n) { return "logs/" + currentTime + (n ? "-" + std::to_string(n) : "") + ".log.txt"; },
        [&](int n) {
            return "logs/" + currentTime + "-p" + std::to_string(pluma::processId()) +
                   (n ? "-" + std::to_string(n) : "") + ".log.txt";
        },
        pluma::claimNewFile, claimed);
    PluginManager::getInstance().setLogFile(mylog);
    if (!PluginManager::logFileOpen())
        std::cout << "[PluMA] Warning: cannot write the log file " << mylog
                  << "; plugin errors will not be logged." << std::endl;
    // Embedded R is started here -- after the log file is open, so its
    // diagnostics are logged -- on the main thread before any kitty.
    // POSIX: R was already started with PluMA, before the plugin libraries
    // were loaded (R::R, as always: C/C++ plugins may use its RInside
    // directly); this only logs, now that the log is open, if that start
    // failed.  Windows: only when R plugins are installed (no
    // MSVC plugin can use RInside; R.dll is loaded only when needed); the
    // first R step or languageLoad("R") starts it otherwise.
    if (PluginManager::languageEnabled("R")) {
#ifdef _WIN32
        bool rPlugins = false;
        std::map<std::string, std::string>::const_iterator itr;
        const std::map<std::string, std::string>& langs = PluginManager::getInstance().pluginLanguages;
        for (itr = langs.begin(); itr != langs.end() && !rPlugins; itr++)
            rPlugins = itr->second == "R";
        if (rPlugins) PluginManager::languageLoad("R");
#else
        PluginManager::languageLoad("R");
#endif
    }

    /////////////////////////////////////////////////////////////////////
    // Read configuration file and make appropriate plugins
    const bool restartReached = readConfigGuarded(std::string(argv[1]), "", restart);
    // A restart point that never matched (e.g. misspelled) skipped every
    // step: report it and fail, rather than "succeed" having run nothing.
    if (restart && !restartReached && !pipelineFailed) {
        const std::string msg = "Restart point " + *restart + " not found in " + argv[1] +
                                " (or its nested pipelines); nothing was run.";
        PluginManager::getInstance().log(msg);
        std::cout << "[PluMA] " << msg << std::endl;
        pipelineFailed = true;
    }
    // The pipeline is over: code still run on this thread (e.g. Python atexit
    // handlers during the shutdown below) is no longer inside a step.
    PluginManager::clearOnPipelineThread();
    // All readConfig() calls and their threads have finished; the cleanup
    // below runs for both outcomes, and the exit status reports failure.
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // Cleanup: shut down the embedded interpreters (flushes Python buffers,
    // runs atexit handlers, destroys the JVM) -- also after a failure.
    // (unloadAtExit: never waits for threads a plugin left running.)  In
    // reverse order of registration, so Python -- registered right after C --
    // goes last: R's and Perl's exit code (.Last, END blocks, finalizers) may
    // still call into it (reticulate, Inline::Python).
    for (size_t i = PluginManager::supported.size(); i-- > 0;)
        PluginManager::supported[i]->unloadAtExit();
    /////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // El Finito!
    return pipelineFailed || stepFailedContinued ? 1 : 0;
    /////////////////////////////////////////////////////////////////////
}
