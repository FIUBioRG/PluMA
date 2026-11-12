/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows only: the PluMA plugin API (PluginWrapper.h) for PerlPluMA, which
   is built with Strawberry Perl's g++ and so cannot call PluginManager's
   MSVC C++ interface.  It calls the plain-C pluma_host_* functions that
   pluma.exe exports (src/PluginManager.cxx), found in the process's own
   executable. */
#include "PluginWrapper.h"

#include <windows.h>

#include <stdexcept>

namespace {

struct HostApi {
    void (*log)(const char*);
    void (*dependency)(const char*);
    const char* (*prefix)(void);
    void (*languageLoad)(const char*);
    void (*languageUnload)(const char*);
};

// Resolved once, thread-safely (a function-local static's initializer runs
// exactly once): Perl threads may call the API at the same time on first use.
const HostApi* resolveHostApi() {
    static HostApi api;
    HMODULE exe = GetModuleHandleA(NULL);
    typedef int (*VersionFn)(void);
    VersionFn version = (VersionFn) GetProcAddress(exe, "pluma_host_api_version");
    api.log = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_log");
    api.dependency = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_dependency");
    api.prefix = (const char* (*)(void)) GetProcAddress(exe, "pluma_host_prefix");
    api.languageLoad = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_language_load");
    api.languageUnload = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_language_unload");
    const bool found = version && version() == 1 && api.log && api.dependency && api.prefix && api.languageLoad &&
                       api.languageUnload;
    return found ? &api : NULL;
}

const HostApi& requireHost() {
    static const HostApi* const api = resolveHostApi();
    if (!api) throw std::runtime_error("the PluMA plugin API is only available inside the PluMA process");
    return *api;
}

}  // namespace

void log(char* msg) { requireHost().log(msg); }

void dependency(char* plugin) { requireHost().dependency(plugin); }

char* prefix() { return (char*) requireHost().prefix(); }

void languageLoad(char* lang) { requireHost().languageLoad(lang); }

void languageUnload(char* lang) { requireHost().languageUnload(lang); }
