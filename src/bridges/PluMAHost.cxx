/* Copyright (C) 2026 FIUBioRG
   SPDX-License-Identifier: MIT

   Windows only: the PluMA plugin API (PluginWrapper.h) for PerlPluMA, which
   is built with Strawberry Perl's g++ and so cannot call PluginManager's
   MSVC C++ interface.  It calls the plain-C pluma_host_* functions that
   pluma.exe exports (src/PluginManager.cxx), found in the process's own
   executable; anywhere else (a plain perl.exe) the API reports that it is
   only available inside PluMA, as src/PluMA.cxx does. */
#include "PluginWrapper.h"

#include <windows.h>

#include <stdexcept>
#include <string>

namespace {

struct HostApi {
    int (*inHostProcess)(void);
    void (*log)(const char*);
    int (*dependency)(const char*, char*, int);
    const char* (*prefix)(void);
    void (*languageLoad)(const char*);
    void (*languageUnload)(const char*);
};

// Resolved once, thread-safely (a function-local static's initializer runs
// exactly once, other callers waiting for it): Perl threads may call the API
// at the same time on first use.
const HostApi* resolveHostApi() {
    static HostApi api;
    bool found = false;
    {
        HMODULE exe = GetModuleHandleA(NULL);
        typedef int (*VersionFn)(void);
        VersionFn version = (VersionFn) GetProcAddress(exe, "pluma_host_api_version");
        api.inHostProcess = (int (*)(void)) GetProcAddress(exe, "pluma_host_in_host_process");
        api.log = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_log");
        api.dependency = (int (*)(const char*, char*, int)) GetProcAddress(exe, "pluma_host_dependency");
        api.prefix = (const char* (*)(void)) GetProcAddress(exe, "pluma_host_prefix");
        api.languageLoad = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_language_load");
        api.languageUnload = (void (*)(const char*)) GetProcAddress(exe, "pluma_host_language_unload");
        found = version && version() == 1 && api.inHostProcess && api.log && api.dependency && api.prefix &&
                api.languageLoad && api.languageUnload && api.inHostProcess();
    }
    return found ? &api : NULL;
}

const HostApi* hostApi() {
    static const HostApi* const api = resolveHostApi();
    return api;
}

const HostApi& requireHost() {
    const HostApi* api = hostApi();
    if (!api)
        throw std::runtime_error("the PluMA plugin API is only available inside the PluMA process");
    return *api;
}

}  // namespace

void log(char* msg) { requireHost().log(msg); }

// Throws when the dependency is not met; the SWIG %exception handler in
// PluginWrapper.i turns that into a Perl die().
void dependency(char* plugin) {
    char err[1024] = "";
    if (requireHost().dependency(plugin, err, sizeof(err)) != 0) throw std::runtime_error(err);
}

char* prefix() { return (char*) requireHost().prefix(); }

void languageLoad(char* lang) { requireHost().languageLoad(lang); }

void languageUnload(char* lang) { requireHost().languageUnload(lang); }
