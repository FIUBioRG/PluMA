/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   The plain-C contract between PluMA and its Perl runner (languages/PerlRun.h).
   On POSIX the runner is compiled into pluma; on Windows into pluma_perl.dll
   with Strawberry Perl's g++ (MSVC cannot compile Perl's headers), which
   pluma.exe loads the first time a Perl plugin runs.  Only C types cross:
   no C++ object or exception passes between the two compilers. */
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
    /* PluginManager::takeDependencyFailed(): reads and clears this thread's
       "unmet dependency()" mark. */
    int (*takeDependencyFailed)(void);
};

enum PerlRunStatus {
    PERL_RUN_OK = 0,           /* completed, or ended with exit(0)            */
    PERL_RUN_NOT_STARTED = 1,  /* PluginNotStarted: existing output is kept   */
    PERL_RUN_FAILED = 2        /* a failure while running: output removed     */
};

typedef int (*PlumaPerlBridgeVersion)(void);
/* Runs one Perl plugin (calls are serialized by pluma.exe).  Returns a
   PerlRunStatus; `message` receives the failure text (NUL-terminated). */
typedef int (*PlumaPerlRun)(const struct PerlHost* host, const char* pluginname, const char* filename,
                            const char* inputname, const char* outputname, char* message, int messagelen);

#ifdef __cplusplus
}
#endif

#endif
