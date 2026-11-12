// Copyright (C) 2026 FIU BioRG
// SPDX-License-Identifier: MIT
//
// Symbol visibility for state shared between the pluma executable and the
// plugins it loads.
//
// On Linux/macOS, pluma is linked with -rdynamic, so plugins resolve
// PluginManager symbols against the executable at dlopen() time and no
// annotation is required.
//
// On Windows a DLL cannot leave symbols unresolved, and each module gets its
// own copy of anything defined inline.  pluma.exe therefore exports
// PluginManager (PLUMA_EXPORTS is defined while compiling the executable) and
// plugins import it through the pluma.lib import library.

#ifndef PLUMA_EXPORT_H
#define PLUMA_EXPORT_H

#if defined(_WIN32)
#  if defined(PLUMA_EXPORTS)
#    define PLUMA_API __declspec(dllexport)
#  else
#    define PLUMA_API __declspec(dllimport)
#  endif
#else
#  define PLUMA_API
#endif

#endif
