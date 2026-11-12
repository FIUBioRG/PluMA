#ifndef PLUMA_JULIA_H
#define PLUMA_JULIA_H

#include "Language.h"

#ifdef HAVE_JULIA
#include <julia.h>
#include <string>
#include <vector>
#endif

class Julia : public Language {
public:
    Julia(std::string language, std::string ext, std::string pp);
    ~Julia();
    void executePlugin(std::string pluginname, std::string inputname, std::string outputname);
    void load();
    void unload();

private:
#ifdef HAVE_JULIA
    bool initialized;

    // <plugin>/<plugin>Plugin.jl, or <plugin>/plugin.jl, from the plugin
    // path; throws PluginNotStarted if there is none.
    std::string findJuliaPluginFile(const std::string& pluginname) const;
    bool callPluginFunction(const std::string& moduleName, const char* funcName,
                           const std::string& arg);
    bool callPluginFunctionNoArgs(const std::string& moduleName, const char* funcName);
#endif
};

#endif
