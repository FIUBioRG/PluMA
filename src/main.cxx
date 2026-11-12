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
//#include "PluginManager.h"
#include "PluginProxy.h"
/*#include "languages/Compiled.h"
#include "languages/Py.h"
#include "languages/R.h"
#include "languages/Perl.h"*/
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <cctype>
#include "platform.h"
using std::thread;
using std::vector;

//////////////////////////////////////////
// Helper Function: Convert int to string
std::string toString(int val) {
   std::string retval;
   std::stringstream ss;
   ss << val;
   ss >> retval;
   return retval;
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

// Removes a failed step's possibly incomplete output -- only a regular
// file, never a directory, device (/dev/null) or FIFO -- logging why; warns
// if it cannot be removed (e.g. on Windows, while a plugin holds it open).
// Returns whether there was such a file to remove.
static bool removeIncompleteOutput(const std::string& output) {
    if (!pluma::isRegularFile(output)) return false;
    PluginManager::log("REMOVING OUTPUT FILE: " + output + ".");
    if (std::remove(output.c_str()) != 0) {
        PluginManager::log("WARNING: could not remove " + output + "; it may be incomplete.");
        std::cerr << "[PluMA] Warning: could not remove possibly incomplete output " << output << std::endl;
    }
    return true;
}

// The pipeline statements and how many arguments each takes -- the
// grammar readConfig() reads.  Any other first word
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

// The words of a pipeline file, as readConfig() reads them.  Statements are read word
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
    explicit PipelineWords(const std::string& file) {
        std::ifstream f(file.c_str(), std::ios::in | std::ios::binary);
        opened = f.is_open() && !pluma::isDirectory(file);
        if (!opened) return;
        std::string data;
        failed = !pluma::readWholeStream(f, data);
        pluma::stripUtf8Bom(data);   // UTF-8 byte order mark
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

// One statement's arguments (see `statements`): `word` is its first word.  Reading stops at the
// first missing argument, which stays empty along with the rest.
static void readStatement(PipelineWords& words, const std::string& word, std::vector<std::string>& args) {
    const Statement* statement = findStatement(word);
    const size_t count = statement ? statement->arguments : 5;   // (a stray word: a Plugin line)
    args.assign(count, std::string());
    for (size_t i = 0; i < count && words.argument(args[i]); ++i) {}
}

//////////////////////////////////////////
struct args {
   char* myFile;
   char* myPrefix;
   bool* myDoRestart;
   char* myRestartPoint;
};

//void readConfig(void* pars) {
//void readConfig(void* infilep, void* pref, void* doR, void* rP) {
void readConfig(std::string inputfile, std::string prefix, bool doRestart, std::string restartPoint) {
    PluginManager::setOnPipelineThread();   // plugin exceptions here are handled below
    PipelineWords words(inputfile);   // (a UTF-8 byte order mark is skipped)
    if (!words.isOpen()) {
        PluginManager::getInstance().log("ERROR: cannot open pipeline file "+inputfile+".");
        std::cout << "[PluMA] Cannot open pipeline file " << inputfile << ". Exiting." << std::endl;
        pipelineFailed = true;
        return;
    }
    bool restartFlag = false;
    std::string pipeline = "";
    std::string oldprefix = prefix;
    std::string kitty = "";
    bool parallelflag = false, kittyflag = false;
    vector<std::thread> threads;
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
            prefix = args[0];
            if (prefix.empty()) { incompleteLine("Prefix", "Prefix <directory>"); break; }
            prefix += "/";
            PluginManager::myPrefix=prefix;
            oldprefix = prefix;
            continue;
        } else if (junk == "Pipeline") {
            pipeline = args[0];
            if (pipeline.empty()) { incompleteLine("Pipeline", "Pipeline <file>"); break; }
	    if (parallelflag && kittyflag) { // Kitty is multithreaded
		threads.push_back(std::thread(readConfig, pipeline, prefix, false, ""));
	    }
	    else {
            readConfig(pipeline, prefix, false, "");
	    }
            continue;   // control line: no plugin to run
        } else if (junk == "Kitty") {
            kitty = args[0];
            if (kitty.empty()) { incompleteLine("Kitty", "Kitty <name>"); break; }
            if (oldprefix != "") {
                prefix = oldprefix;
            }
            prefix += "/"+kitty+"/";
            PluginManager::myPrefix=prefix;
	    kittyflag = true;
            continue;
	}
	else if (junk == "LitterLaunch") {
            parallelflag = true;
            continue;   // control line: no plugin to run
	}
	else if (junk == "LitterGather") {
	    for (int i = 0; i < threads.size(); i++){
	       std::cout << "[PluMA] Gathering Kitty " << i << std::endl;
	       threads[i].join();
	    }
	    threads.clear();
	    parallelflag = false;
	    kittyflag = false;
            continue;   // control line: no plugin to run
        }
        const std::string keyword = junk;   // "Plugin" (or a stray word)
        // Plugin <name> inputfile <file> outputfile <file>; stops at the first
        // missing word, leaving the rest empty.
        std::string inKeyword, outKeyword;
        name = args[0]; inKeyword = args[1]; inputname = args[2]; outKeyword = args[3]; outputname = args[4];

        ////////////////////////////////////////////////////////////////////////////
        // If we are restarting and have not hit that point yet, skip this plugin
        if (doRestart && !restartFlag) {
            if (name == restartPoint) {
                restartFlag = true;
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

        // (Absolute paths include Windows drive paths, C:/data.)
        if (!pluma::isAbsolutePath(inputname)) {
            inputname = prefix + inputname;
        }

        if (!pluma::isAbsolutePath(outputname)) {
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
            const bool removedOutput = (pluginStarted || outputChanged) && removeIncompleteOutput(outputPath);
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
    // Never leave a joinable std::thread behind (e.g. after a failure or a
    // missing LitterGather): destroying one would end the process.
    for (size_t i = 0; i < threads.size(); i++) threads[i].join();
}


int main(int argc, char** argv)
{
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
    // Entries are separated by pluma::PATH_LIST_SEP (':' on POSIX, ';' on
    // Windows, where ':' is part of drive paths).  The current directory from
    // the system, not $PWD (which Windows does not set).
    std::string pluginpath = "/plugins/";
    pluginpath = pluma::currentDirectory() + pluginpath;
    if (getenv("PLUMA_PLUGIN_PATH") != NULL) {
        pluginpath += pluma::PATH_LIST_SEP;
        pluginpath += getenv("PLUMA_PLUGIN_PATH");
    }
    pluginpath += pluma::PATH_LIST_SEP;
    //pluginpath += "/usr/local/bin/plugins/:/usr/bin/plugins/";
    //pluginpath += ":";
    //std::cout << "PluginPath: " << pluginpath << std::endl;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Set PluginManager::supported languages here.
    /*std::vector<Language*> PluginManager::supported;
    std::map<std::string, std::string> PluginManager::getInstance().pluginLanguages;
    #ifdef APPLE
    PluginManager::supported.push_back(new Compiled("C", "dylib", pluginpath, "lib"));
    #else
    PluginManager::supported.push_back(new Compiled("C", "so", pluginpath, "lib"));
    #endif
    PluginManager::supported.push_back(new Py("Python", "py", pluginpath));
    PluginManager::supported.push_back(new MiAMi::R("R", "R", pluginpath, argc, argv));
    PluginManager::supported.push_back(new Perl("Perl", "pl", pluginpath));*/
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
    std::string path = pluginpath.substr(0, pluginpath.find_first_of(pluma::PATH_LIST_SEP));
    bool list = false;
    while (path.length() > 0) {
        //std::cout << "Plugin Path: " << pluginpath << std::endl;
        //std::cout << "Path: " << path << std::endl;
        if (std::string(argv[1]) == "plugins") {
            std::cout << "[PluMA] Current plugin list: " << std::endl;
            list = true;
        }

        for (int i = 0; i < PluginManager::supported.size(); i++)
            PluginManager::supported[i]->loadPlugin(path, &(PluginManager::getInstance().pluginLanguages), list);

        std::map<std::string, std::string>::iterator itr;

        // Only plugins that can actually run count: plugins of a language
        // compiled out of this build are registered just to report "not
        // enabled".
        for (itr = PluginManager::getInstance().pluginLanguages.begin(); itr != PluginManager::getInstance().pluginLanguages.end(); itr++) {
            if (PluginManager::languageEnabled(itr->second))
                PluginManager::getInstance().add(itr->first.substr(0, itr->first.length()-6));
        }
        //globfree(&globbuf);
        pluginpath = pluginpath.substr(pluginpath.find_first_of(pluma::PATH_LIST_SEP)+1, pluginpath.length());
        path = pluginpath.substr(0, pluginpath.find_first_of(pluma::PATH_LIST_SEP));
    }
    if (list) exit(0);
    //////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////
    // Check for a restart point
    bool doRestart = false;
    std::string restartPoint = "";
    if (argc == 3) {
        doRestart = true;
        restartPoint = std::string(argv[2]);
    }
    //////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get the current time, and setup the initial log file
    time_t t = time(0);
    struct tm* now = localtime( &t );
    // (TIMESTAMP_SEP: ':' is not allowed in Windows file names.)
    const std::string sep(1, pluma::TIMESTAMP_SEP);
    std::string currentTime = toString(now->tm_year + 1900) + "-" + toString(now->tm_mon + 1) + "-" + toString(now->tm_mday) + "@" + toString(now->tm_hour) + sep + toString(now->tm_min) + sep + toString(now->tm_sec);
    std::string mylog = "logs/"+currentTime+".log.txt";
    PluginManager::getInstance().setLogFile(mylog);

    /////////////////////////////////////////////////////////////////////
    // Read configuration file and make appropriate plugins
    //args* myArgs = new args();
    //myArgs->myFile = argv[1];
    //myArgs->myPrefix = (char*) "";
    //myArgs->myDoRestart = &doRestart;
    //myArgs->myRestartPoint = (char*) (restartPoint.c_str());
    readConfig(std::string(argv[1]), "", doRestart, restartPoint);
    // The pipeline is over: code still run on this thread (e.g. Python atexit
    // handlers during the shutdown below) is no longer inside a step.
    PluginManager::clearOnPipelineThread();
    //readConfig(myArgs);
    //delete myArgs;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


   /*std::ifstream infile(argv[1], std::ios::in);
   bool restartFlag = false;
   std::string prefix = "";
   while (!infile.eof()) {
      ///////////////////////////////////////////////////////
      // Read one line
      // Plugin (Name) inputfile (input file) outputfile (output file)
      std::string junk, name, inputname, outputname;
      infile >> junk;
      if (junk[0] == '#') {getline(infile, junk); continue;} // comment
      else if (junk == "Prefix") {infile >> prefix; prefix += "/"; continue;} // prefix
      else infile >> name >> junk >> inputname >> junk >> outputname;
      if (inputname[0] != '/')
         inputname = prefix + inputname;
      if (outputname[0] != '/')
         outputname = prefix + outputname;
      //////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////////////////////
      // If we are restarting and have not hit that point yet, skip this plugin
      if (doRestart && !restartFlag)
         if (name == restartPoint)
            restartFlag = true;
         else
           continue;
      /////////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      // Try to create and run all three steps of the plugin in the appropriate language
      PluginManager::getInstance().log("Creating plugin "+name);
      try {
         bool executed = false;
         for (int i = 0; i < PluginManager::supported.size() && !executed; i++)
            if (PluginManager::getInstance().pluginLanguages[name+"Plugin"] == PluginManager::supported[i]->lang()) {
               std::cout << "[PluMA] Running Plugin: " << name << std::endl;
               PluginManager::supported[i]->executePlugin(name, inputname, outputname);
               executed = true;
            }
         ///////////////////////////////////////////////////////////////////////////////////////////////////////
         // In this case we found the plugin, but the language is not PluginManager::supported.
         if (!executed && name != "")
             PluginManager::getInstance().log("Error, no suitable language for plugin: "+name+".");
         ///////////////////////////////////////////////////////////////////////////////////////////////////////
      }
      ///////////////////////////////////////////////////////////////////////////////////////////////////////
      // This hits if the plugin errored while running it
      // Message(s) will be output to the logfile, and output files will be removed.
      catch (...) {
         PluginManager::getInstance().log("ERROR IN PLUGIN: "+name+".");;
         if (access( outputname.c_str(), F_OK ) != -1 ) {
            PluginManager::getInstance().log("REMOVING OUTPUT FILE: "+outputname+".");
            system(("rm "+outputname).c_str());
            exit(1);
	    }
      }
      std::cout << "HERE" << std::endl;
      ////////////////////////////////////////////////////////////////////////////////////////////////////////
   }*/
   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // Cleanup -- also after a failure; the exit status reports it.
    for (int i = 0; i < PluginManager::supported.size(); i++)
        PluginManager::supported[i]->unload();
    /////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////
    // El Finito!
    return pipelineFailed || stepFailedContinued ? 1 : 0;
    /////////////////////////////////////////////////////////////////////
}
