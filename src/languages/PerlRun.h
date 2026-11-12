/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows: the Perl plugin runner, compiled with Strawberry Perl's g++ into
   pluma_perl.dll (src/bridges/PerlBridge.cxx) -- Strawberry Perl is built
   with GCC and its headers cannot be compiled by MSVC.  It does what
   Perl.cxx does on POSIX: one interpreter per plugin run, then its input(),
   run() and output().  It talks to pluma.exe through plain C (PerlHost). */
#ifndef PLUMA_PERLRUN_H
#define PLUMA_PERLRUN_H

#include <string>

#include "../bridges/PerlBridge.h"   // PerlHost

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

static void perlRunPlugin(const PerlHost& host, const std::string& pluginname, const std::string& filename,
                          const std::string& inputname, const std::string& outputname)
{
    auto log = [&](const std::string& msg) { host.log(msg.c_str()); };
    char* args_input[] = { (char*) inputname.c_str(), NULL };
    char* args_run[] = { NULL };
    char* args_output[] = { (char*) outputname.c_str(), NULL };
    static char emptyArg[] = "";
    char* argv[] = { emptyArg, (char*) filename.c_str(), NULL };
    int argc = 2;
    my_perl = perl_alloc();
    perl_construct(my_perl);
    perl_parse(my_perl, xs_init, argc, argv, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    /*** skipping perl_run() ***/
    log("Executing input() For Perl Plugin " + pluginname);
    call_argv("input", G_DISCARD, args_input);
    log("Executing run() For Perl Plugin " + pluginname);
    call_argv("run", G_DISCARD | G_NOARGS, args_run);
    log("Executing output() For Perl Plugin " + pluginname);
    call_argv("output", G_DISCARD, args_output);
    perl_destruct(my_perl);
    perl_free(my_perl);
    log("Perl Plugin " + pluginname + " completed successfully.");
}

#endif
