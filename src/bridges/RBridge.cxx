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
#include <cstring>
#include <string>

#define R_NO_REMAP
#include <Rinternals.h>
#include <Rembedded.h>
#include <R_ext/Parse.h>

// Declared in Rinterface.h, which R for Windows does not ship, and (the
// parser's diagnostics, exported by R.dll) in R's internal Parse.h.
extern "C" {
__declspec(dllimport) extern uintptr_t R_CStackLimit;
__declspec(dllimport) extern Rboolean R_Interactive;
__declspec(dllimport) extern int R_ParseError;        // line of the last parse error
__declspec(dllimport) extern char R_ParseErrorMsg[];  // its message (NUL-terminated)
}

#define BRIDGE_API extern "C" __declspec(dllexport)

static void setError(char* err, int errlen, const std::string& message) {
    if (!err || errlen <= 0) return;
    std::snprintf(err, (size_t) errlen, "%s", message.c_str());
}

// Parses and evaluates `code` in the global environment; the value of the
// last expression (protected once: the caller UNPROTECTs 1), or NULL after an
// error, with `err` set.
struct ParseCall {
    SEXP source;
    ParseStatus status;
    SEXP exprs;
};
static void parseInToplevel(void* data) {
    ParseCall* call = static_cast<ParseCall*>(data);
    call->exprs = R_ParseVector(call->source, -1, &call->status, R_NilValue);
}

static SEXP evalCode(const char* code, char* err, int errlen) {
    SEXP text = PROTECT(Rf_mkCharCE(code, CE_UTF8));
    SEXP source = PROTECT(Rf_ScalarString(text));
    // The parser can raise an R error (e.g. "invalid multibyte character",
    // a script not saved as UTF-8) rather than return a status; R's error
    // handling would longjmp to the top level set up by Rf_initEmbeddedR,
    // whose frame is gone.  R_ToplevelExec catches it (R prints the message).
    ParseCall call = { source, PARSE_NULL, R_NilValue };
    if (!R_ToplevelExec(parseInToplevel, &call)) {
        UNPROTECT(2);
        setError(err, errlen, "Parse error: R could not read the script (e.g. characters not valid in UTF-8; "
                              "save it as UTF-8)");
        return NULL;
    }
    ParseStatus status = call.status;
    SEXP exprs = PROTECT(call.exprs);
    if (status != PARSE_OK) {
        UNPROTECT(3);
        // R prints nothing for R_ParseVector failures: report where and why.
        std::string message = status == PARSE_INCOMPLETE ? std::string("incomplete expression at end of input")
                                                         : std::string(R_ParseErrorMsg);
        setError(err, errlen, "Parse error at line " + std::to_string(R_ParseError) + ": " + message);
        return NULL;
    }
    SEXP value = R_NilValue;
    for (R_xlen_t i = 0; i < Rf_xlength(exprs); ++i) {
        int failed = 0;
        value = R_tryEval(VECTOR_ELT(exprs, i), R_GlobalEnv, &failed);
        if (failed) {
            UNPROTECT(3);
            setError(err, errlen, std::string("Error evaluating: ") + code);
            return NULL;
        }
    }
    PROTECT(value);
    UNPROTECT_PTR(exprs);
    UNPROTECT_PTR(source);
    UNPROTECT_PTR(text);
    return value;
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

BRIDGE_API int pluma_r_eval(const char* code, char* err, int errlen) {
    SEXP value = evalCode(code, err, errlen);
    if (!value) return 1;
    UNPROTECT(1);
    return 0;
}

BRIDGE_API int pluma_r_eval_bool(const char* code, int* result, char* err, int errlen) {
    SEXP value = evalCode(code, err, errlen);
    if (!value) return 1;
    const int logical = Rf_asLogical(value);
    UNPROTECT(1);
    if (logical == NA_LOGICAL) {
        setError(err, errlen, std::string("Not a logical value: ") + code);
        return 1;
    }
    *result = logical ? 1 : 0;
    return 0;
}

BRIDGE_API int pluma_r_eval_string(const char* code, char* buf, int buflen, char* err, int errlen) {
    SEXP value = evalCode(code, err, errlen);
    if (!value) return 1;
    int rc = 0;
    if (TYPEOF(value) == STRSXP && Rf_xlength(value) > 0 && STRING_ELT(value, 0) != NA_STRING) {
        setError(buf, buflen, Rf_translateCharUTF8(STRING_ELT(value, 0)));
    } else {
        setError(err, errlen, std::string("Not a character value: ") + code);
        rc = 1;
    }
    UNPROTECT(1);
    return rc;
}

BRIDGE_API int pluma_r_assign_string(const char* name, const char* value, char* err, int errlen) {
    (void) err;
    (void) errlen;
    SEXP str = PROTECT(Rf_ScalarString(Rf_mkCharCE(value, CE_UTF8)));
    Rf_defineVar(Rf_install(name), str, R_GlobalEnv);
    UNPROTECT(1);
    return 0;
}

// Calls the function `name` as found from `env` (R_dot_Last's lookup:
// get0(name, env, "function"), which also searches enclosing frames), if
// there is one, with errors caught (R prints them).
static void callHook(const char* name, SEXP env) {
    // Each argument protected while the next is allocated (Rf_mkString and
    // Rf_lang4 allocate, so an unprotected string could be collected).
    SEXP nameArg = PROTECT(Rf_mkString(name));
    SEXP modeArg = PROTECT(Rf_mkString("function"));
    SEXP lookup = PROTECT(Rf_lang4(Rf_install("get0"), nameArg, env, modeArg));
    int failed = 0;
    SEXP fn = R_tryEval(lookup, R_BaseEnv, &failed);
    if (!failed && Rf_isFunction(fn)) {
        PROTECT(fn);
        SEXP call = PROTECT(Rf_lang1(fn));
        R_tryEval(call, R_GlobalEnv, &failed);
        UNPROTECT(2);
    }
    UNPROTECT(3);
}

BRIDGE_API void pluma_r_end(void) {
    // RInside's shutdown: the .Last hooks, then R's own end of an embedded
    // session (exit finalizers, devices, temporary directory).  The hooks
    // as R_dot_Last runs them -- a user's .Last, then base's .Last.sys -- but
    // with errors caught: R_dot_Last resets R's top-level context to the
    // session's own before calling them, so an error in a plugin's .Last
    // longjmps into the frame Rf_initEmbeddedR set up, long gone (a crash).
    callHook(".Last", R_GlobalEnv);
    callHook(".Last.sys", R_BaseNamespace);
    Rf_endEmbeddedR(0);
}
