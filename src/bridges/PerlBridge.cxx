/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows only: pluma_perl.dll, the Perl runner (see PerlBridge.h), built by
   SConscript.windows with Strawberry Perl's g++ and ExtUtils::Embed flags. */
#include "../languages/PerlRun.h"

#include <cstdio>
#include <exception>

#define BRIDGE_API extern "C" __declspec(dllexport)

BRIDGE_API int pluma_perl_bridge_version(void) { return PLUMA_PERL_BRIDGE_VERSION; }

BRIDGE_API int pluma_perl_run(const PerlHost* host, const char* pluginname, const char* filename,
                              const char* inputname, const char* outputname, char* message, int messagelen) {
    // Once per process, before the first interpreter (as Perl's own main()).
    static bool initialized = false;
    if (!initialized) {
        static char arg0[] = "pluma";
        static char* args[] = { arg0, NULL };
        static char* envs[] = { NULL };
        // Static storage, as in Perl.cxx: PERL_SYS_INIT3 may keep or replace
        // the pointers it is given.
        static int argc = 1;
        static char** argv = args;
        static char** env = envs;
        PERL_SYS_INIT3(&argc, &argv, &env);
        initialized = true;
    }
    std::string text;
    int status;
    try {
        status = perlRunPlugin(*host, pluginname, filename, inputname, outputname, text);
    } catch (const std::exception& e) {   // never let a g++ exception reach MSVC code
        status = PERL_RUN_FAILED;
        text = std::string("Perl plugin ") + pluginname + ": " + e.what();
    } catch (...) {
        status = PERL_RUN_FAILED;
        text = std::string("Perl plugin ") + pluginname + ": unexpected error in the Perl runner";
    }
    if (message && messagelen > 0) std::snprintf(message, (size_t) messagelen, "%s", text.c_str());
    return status;
}
