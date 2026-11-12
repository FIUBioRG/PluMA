// Copyright (C) 2026 FIU BioRG
// SPDX-License-Identifier: MIT
//
// <sys/time.h> for C++/CUDA plugins built on Windows (on the plugin include
// path only; SConscript.windows): struct timeval and gettimeofday(), which
// plugins written on POSIX use to time their work.  Built on <chrono>, so it
// pulls in no <windows.h>.  The time zone argument is ignored, as on modern
// POSIX systems.  Winsock defines struct timeval too, with no guard: a
// plugin that uses it includes <winsock2.h> (or <windows.h> without
// WIN32_LEAN_AND_MEAN) before this header, which then uses winsock's.  (Not
// included from here: it would give every such plugin <windows.h> and its
// macros -- min/max, ERROR -- which clash with POSIX code.)

#ifndef PLUMA_COMPAT_SYS_TIME_H
#define PLUMA_COMPAT_SYS_TIME_H

#ifdef __cplusplus
#include <chrono>

#if !defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_) && !defined(_TIMEVAL_DEFINED)
#define _TIMEVAL_DEFINED
// As winsock's own struct timeval (which a plugin may also get): `long`,
// 32 bits on Windows -- seconds since 1970 fit until 2038, as with winsock.
// Plugins use it to time their work (differences stay correct).
struct timeval {
    long tv_sec;    // seconds
    long tv_usec;   // microseconds
};
#endif

inline int gettimeofday(struct timeval* tv, void* /* tz */) {
    if (!tv) return 0;
    const long long us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
    tv->tv_sec = static_cast<long>(us / 1000000);
    tv->tv_usec = static_cast<long>(us % 1000000);
    return 0;
}
#endif

#endif
