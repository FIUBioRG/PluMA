/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows only: pluma_r.dll, the embedded-R bridge (see RBridge.h).  Built
   by SConscript.windows with MSVC against R's headers (R_LEGACY_RCOMPLEX:
   MSVC has no C99 _Complex; the layout is the same) and an import library
   generated from R.dll. */
#include "RBridge.h"

// Standard headers first, and R_NO_REMAP: R otherwise #defines short names
// such as `warning` (-> Rf_warning), which breaks MSVC's `#pragma warning`.
#include <cstdint>
#include <cstdio>
#include <string>

#define R_NO_REMAP
#include <Rinternals.h>
#include <Rembedded.h>
#include <R_ext/Parse.h>

// Declared in Rinterface.h, which R for Windows does not ship.
extern "C" {
__declspec(dllimport) extern uintptr_t R_CStackLimit;
__declspec(dllimport) extern Rboolean R_Interactive;
}

#define BRIDGE_API extern "C" __declspec(dllexport)

static void setError(char* err, int errlen, const std::string& message) {
    if (!err || errlen <= 0) return;
    std::snprintf(err, (size_t) errlen, "%s", message.c_str());
}

BRIDGE_API int pluma_r_bridge_version(void) { return PLUMA_R_BRIDGE_VERSION; }

BRIDGE_API int pluma_r_start(int argc, char** argv, char* err, int errlen) {
    (void) argc;
    (void) argv;
    // RInside's defaults: no saved workspace, quiet, no site/user profiles.
    static char arg0[] = "R", arg1[] = "--no-save", arg2[] = "--silent", arg3[] = "--vanilla";
    char* args[] = { arg0, arg1, arg2, arg3 };
    if (Rf_initEmbeddedR(4, args) < 0) {
        setError(err, errlen, "R could not be initialized (check R_HOME)");
        return 1;
    }
    // As RInside does: R is called from PluMA's kitty threads too (serialized),
    // whose stacks R's C-stack check would treat as overflows; and a
    // non-interactive session, as for Rscript.
    R_CStackLimit = (uintptr_t) -1;
    R_Interactive = FALSE;
    return 0;
}

// Parses `code` and evaluates each expression in the global environment, as
// RInside::parseEvalQ does.
BRIDGE_API int pluma_r_eval(const char* code, char* err, int errlen) {
    SEXP source = PROTECT(Rf_mkString(code));
    ParseStatus status;
    SEXP exprs = PROTECT(R_ParseVector(source, -1, &status, R_NilValue));
    if (status != PARSE_OK) {
        UNPROTECT(2);
        setError(err, errlen, std::string("Parse error in: ") + code);
        return 1;
    }
    for (R_xlen_t i = 0; i < Rf_xlength(exprs); ++i) {
        int failed = 0;
        R_tryEval(VECTOR_ELT(exprs, i), R_GlobalEnv, &failed);
        if (failed) {
            UNPROTECT(2);
            setError(err, errlen, std::string("Error evaluating: ") + code);
            return 1;
        }
    }
    UNPROTECT(2);
    return 0;
}

BRIDGE_API void pluma_r_end(void) {
    Rf_endEmbeddedR(0);
}
