/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows only: pluma_perl.dll, the Perl runner (see PerlBridge.h), built by
   SConscript.windows with Strawberry Perl's g++ and ExtUtils::Embed flags. */
#include "../languages/PerlRun.h"

#define BRIDGE_API extern "C" __declspec(dllexport)

BRIDGE_API int pluma_perl_bridge_version(void) { return PLUMA_PERL_BRIDGE_VERSION; }

BRIDGE_API void pluma_perl_run(const PerlHost* host, const char* pluginname, const char* filename,
                               const char* inputname, const char* outputname) {
    // Once per process, before the first interpreter (as Perl's own main()).
    // Static storage: PERL_SYS_INIT3 may keep or replace the pointers it is given.
    static bool initialized = false;
    if (!initialized) {
        static char arg0[] = "pluma";
        static char* args[] = { arg0, NULL };
        static char* envs[] = { NULL };
        static int argc = 1;
        static char** argv = args;
        static char** env = envs;
        PERL_SYS_INIT3(&argc, &argv, &env);
        initialized = true;
    }
    try {
        perlRunPlugin(*host, pluginname, filename, inputname, outputname);
    } catch (...) {   // never let a g++ exception reach MSVC code
        host->log((std::string("Perl plugin ") + pluginname + ": unexpected error in the Perl runner").c_str());
    }
}
