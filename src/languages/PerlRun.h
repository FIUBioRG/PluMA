/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   The Perl plugin protocol: one interpreter per plugin run, its phases, and
   how die() is classified.  Included by exactly one translation unit
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
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    /*** skipping perl_run() ***/

    // Runs one phase.  G_EVAL catches die() -- including the croak raised by
    // PerlPluMA::dependency() for an unmet dependency.  Returns PERL_RUN_OK
    // to go on, or a failure status.
    auto callPhase = [&](const char* phase, I32 flags, char** args) -> int {
        const bool isInput = std::string(phase) == "input";
        log(std::string("Executing ") + phase + "() For Perl Plugin " + pluginname);
        call_argv(phase, flags | G_DISCARD | G_EVAL, args);
        if (SvTRUE(ERRSV)) {
            std::string err = SvPV_nolen(ERRSV);
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
    if (status != PERL_RUN_OK) return status;    // failed (interpreter already destroyed)
    log("Perl Plugin " + pluginname + " completed successfully.");
    return PERL_RUN_OK;
}

#endif
