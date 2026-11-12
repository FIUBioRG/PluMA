/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows: the C interface of pluma_r.dll, the embedded-R bridge.

   R for Windows is built with GCC and RInside/Rcpp (used on POSIX) are C++
   built against it, so MSVC cannot use them.  R's own C API works from MSVC,
   but R.dll exports data (R_GlobalEnv, R_NilValue, ...), which pluma.exe
   cannot import from a delay-loaded DLL -- and pluma.exe must start on
   machines without R.  So the R calls live in pluma_r.dll, which links R.dll
   normally; pluma.exe loads it the first time R is needed (after loading
   R.dll by absolute path) and calls these functions through GetProcAddress.

   Every function is noexcept (plain C): errors come back as a nonzero
   return value with a message in `err` (always NUL-terminated). */
#ifndef PLUMA_RBRIDGE_H
#define PLUMA_RBRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped on any change below; pluma.exe refuses a bridge of another version. */
#define PLUMA_R_BRIDGE_VERSION 1

typedef int (*PlumaRBridgeVersion)(void);
/* Starts embedded R (R_HOME must be set).  0 on success. */
typedef int (*PlumaRStart)(int argc, char** argv, char* err, int errlen);
/* Parses and evaluates `code` in the global environment, discarding the
   value.  0 on success; on an R error (which R has already printed), 1 and
   "Error evaluating: <code>" in `err`, as RInside reports it. */
typedef int (*PlumaREval)(const char* code, char* err, int errlen);
/* Ends embedded R; not restartable. */
typedef void (*PlumaREnd)(void);

#ifdef __cplusplus
}
#endif

#endif
