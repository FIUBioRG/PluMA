// Copyright (C) 2020 FIU BioRG
// SPDX-License-Identifier: MIT

%module PluMA_Wrapper
%{
#include "PluginWrapper.h"
#include <stdexcept>
%}

// PluMA functions report failures (e.g. an unmet dependency()) by throwing.
// Turn those into exceptions of the plugin's own language so the plugin
// aborts immediately and the failure reaches PluMA's pipeline runner,
// instead of a C++ exception unwinding through the interpreter.
%include "exception.i"
%exception {
    try {
        $action
    } catch (const std::exception& e) {
#ifdef SWIGR
        // R's SWIG_Error takes a printf format: pass the message as an
        // argument, never as the format ("50%sFoo" must not read varargs).
        SWIG_Error(SWIG_RuntimeError, "%s", e.what());
        SWIG_fail;
#else
        SWIG_exception(SWIG_RuntimeError, e.what());
#endif
    } catch (...) {
        // Never let a C++ exception unwind through the interpreter's frames.
#ifdef SWIGR
        SWIG_Error(SWIG_RuntimeError, "%s", "unknown C++ exception");
        SWIG_fail;
#else
        SWIG_exception(SWIG_RuntimeError, "unknown C++ exception");
#endif
    }
}

%include "PluginWrapper.h"
