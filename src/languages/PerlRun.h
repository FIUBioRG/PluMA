/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   The Perl plugin protocol: one interpreter per plugin run, its phases, and
   how exit()/die() are classified.  Included by exactly one translation unit
   per binary: Perl.cxx in pluma (POSIX), or src/bridges/PerlBridge.cxx in
   pluma_perl.dll (Windows).  On Windows, Perl is Strawberry Perl, built with
   GCC -- its headers cannot be compiled by MSVC -- so the code that touches
   the Perl API is built with Strawberry's g++ into pluma_perl.dll, and talks
   to pluma.exe through plain C (PerlHost callbacks in, a status code out):
   no C++ object or exception crosses between the two compilers.

   Requires HAVE_PERL, EXTERN.h/perl.h, and a caller that serializes runs
   (one interpreter at a time). */
#ifndef PLUMA_PERLRUN_H
#define PLUMA_PERLRUN_H

#include <string>

#include "../bridges/PerlBridge.h"   // PerlHost, PerlRunStatus

#include <EXTERN.h>
#include <perl.h>
EXTERN_C void xs_init(pTHX);
EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);

EXTERN_C void xs_init(pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;

    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

static PerlInterpreter* my_perl;

/* Runs the Perl plugin `pluginname` from `filename`.  `message` receives the
   exception text for PERL_RUN_NOT_STARTED / PERL_RUN_FAILED. */
static int perlRunPlugin(const PerlHost& host, const std::string& pluginname, const std::string& filename,
                         const std::string& inputname, const std::string& outputname, std::string& message)
{
    auto log = [&](const std::string& msg) { host.log(msg.c_str()); };
    char* args_input[] = { (char*) inputname.c_str(), NULL };
    char* args_run[] = { NULL };
    char* args_output[] = { (char*) outputname.c_str(), NULL };
    static char emptyArg[] = "";
    char* argv[] = { emptyArg, (char*) filename.c_str(), NULL };
    int argc = 2;
    my_perl = perl_alloc();
    PERL_SET_CONTEXT(my_perl);
    perl_construct(my_perl);
    auto destroyInterpreter = [&]() {
        perl_destruct(my_perl);
        perl_free(my_perl);
        my_perl = NULL;
    };
    if (perl_parse(my_perl, xs_init, argc, argv, NULL) != 0) {
        // Syntax error, missing `use` module, ...: Perl has printed why.
        destroyInterpreter();
        log("Perl Plugin " + pluginname + " failed to compile.");
        message = "Perl plugin " + pluginname + " failed to compile";
        return PERL_RUN_NOT_STARTED;
    }
    // Older Perls' perl_parse() also returns 0 when a BEGIN block (or a module
    // it loads) called exit(0) -- current ones report it as a failure, above.
    // The rest of the script was never compiled, so there is no main program:
    // not a plugin that can run, as for Python's exit during import.
    if (!PL_main_root) {
        destroyInterpreter();
        log("Perl Plugin " + pluginname + " called exit() while compiling.");
        message = "Perl plugin " + pluginname + " called exit() while compiling";
        return PERL_RUN_NOT_STARTED;
    }
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    /*** skipping perl_run() ***/

    // Runs one phase.  G_EVAL catches die() -- including the croak raised by
    // PerlPluMA::dependency() for an unmet dependency.  Perl's exit() is not
    // a die(): it long-jumps to the innermost JMPENV and, without one (we
    // skip perl_run()), exits the whole process, possibly from a kitty
    // thread.  So each call gets its own JMPENV, as perl_run() would provide.
    // exit(0) ends the plugin successfully (remaining phases skipped); any
    // other status, or a die(), is a plugin failure.  Returns PERL_RUN_OK to
    // go on, -1 when the plugin exited cleanly, or a failure status.
    auto callPhase = [&](const char* phase, I32 flags, char** args) -> int {
        const bool isInput = std::string(phase) == "input";
        // Missing subs: input is required (nothing ran yet: not started);
        // run/output are optional, as for Python, Java, R and the C++ base.
        if (!get_cv(phase, 0)) {
            log("Perl Plugin " + pluginname + " has no " + phase + "()" + (isInput ? "." : "; skipped."));
            if (!isInput) return PERL_RUN_OK;
            destroyInterpreter();
            message = "Perl plugin " + pluginname + " has no input()";
            return PERL_RUN_NOT_STARTED;
        }
        log(std::string("Executing ") + phase + "() For Perl Plugin " + pluginname);
        int jumped;
        dJMPENV;
        JMPENV_PUSH(jumped);
        if (jumped == 0)
            call_argv(phase, flags | G_DISCARD | G_EVAL, args);
        JMPENV_POP;
        if (jumped == 2) {                       // my_exit(): the plugin called exit()
            const int status = STATUS_EXIT;
            destroyInterpreter();
            log("Perl Plugin " + pluginname + " called exit(" + std::to_string(status) + ") during " + phase + "().");
            if (status == 0) return -1;
            message = "Perl plugin " + pluginname + " exited with status " + std::to_string(status);
            return PERL_RUN_FAILED;
        }
        if (jumped != 0 || SvTRUE(ERRSV)) {
            std::string err = jumped == 0 ? SvPV_nolen(ERRSV) : "unexpected Perl unwind";
            destroyInterpreter();
            log("Perl Plugin " + pluginname + " died during " + phase + "(): " + err);
            message = "Perl plugin " + pluginname + " failed during " + phase;
            // input() dying because of an unmet PerlPluMA::dependency(): not started.
            if (isInput && host.takeDependencyFailed()) return PERL_RUN_NOT_STARTED;
            return PERL_RUN_FAILED;
        }
        return PERL_RUN_OK;
    };
    int status = callPhase("input", 0, args_input);
    if (status == PERL_RUN_OK) status = callPhase("run", G_NOARGS, args_run);
    if (status == PERL_RUN_OK) status = callPhase("output", 0, args_output);
    if (status == PERL_RUN_OK) destroyInterpreter();
    if (status > 0) return status;               // failed (interpreter already destroyed)
    log("Perl Plugin " + pluginname + " completed successfully.");
    return PERL_RUN_OK;
}

#endif
