/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows: the plain-C contract between pluma.exe and its Perl runner
   (languages/PerlRun.h), built with Strawberry Perl's g++ into
   pluma_perl.dll (MSVC cannot compile Perl's headers), which pluma.exe loads
   the first time a Perl plugin runs.  Only C types cross: no C++ object or
   exception passes between the two compilers. */
#ifndef PLUMA_PERLBRIDGE_H
#define PLUMA_PERLBRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped on any change below; pluma.exe refuses a bridge of another version. */
#define PLUMA_PERL_BRIDGE_VERSION 1

/* What the runner needs from PluMA.  The callbacks never throw. */
struct PerlHost {
    void (*log)(const char* message);
};

typedef int (*PlumaPerlBridgeVersion)(void);
/* Runs one Perl plugin (calls are serialized by pluma.exe). */
typedef void (*PlumaPerlRun)(const struct PerlHost* host, const char* pluginname, const char* filename,
                             const char* inputname, const char* outputname);

#ifdef __cplusplus
}
#endif

#endif
