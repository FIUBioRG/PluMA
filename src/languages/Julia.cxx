#include "Julia.h"
#include "../PluginManager.h"
#include "../platform.h"
#include <fstream>
#include <stdexcept>

#ifdef HAVE_JULIA
#include <sstream>
#include <cstring>
#endif

Julia::Julia(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp)
#ifdef HAVE_JULIA
  , initialized(false)
#endif
{}

Julia::~Julia()
{
    unload();
}

void Julia::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_JULIA
    // Find the plugin file (before anything else, as for every language).
    std::string pluginFile = findJuliaPluginFile(pluginname);

    // The embedded Julia runtime may only be called from the thread that
    // started it (other threads would have to be adopted first): Julia steps
    // run on PluMA's main thread, not on parallel kitties.
    if (!PluginManager::isMainThread()) {
        PluginManager::getInstance().log("Julia plugin " + pluginname + " cannot run on a parallel kitty; "
                                         "Julia steps run on PluMA's main thread only.");
        throw PluginNotStarted("Julia plugins run on PluMA's main thread only");
    }

    if (!initialized) load();
    if (!initialized) {
        PluginManager::getInstance().log("Julia runtime is not available; cannot run " + pluginname);
        throw PluginNotStarted("Julia runtime is not available");
    }

    // Create a unique module name for this plugin
    std::string moduleName = pluginname + "Plugin";

    // Load and execute the plugin file as a module
    std::string loadCode =
        "module " + moduleName + "\n"
        "include(\"" + pluginFile + "\")\n"
        "end";

    jl_value_t* result = jl_eval_string(loadCode.c_str());
    if (jl_exception_occurred()) {
        jl_value_t* ex = jl_exception_occurred();
        const char* err_msg = jl_typeof_str(ex);
        PluginManager::getInstance().log("Julia plugin " + pluginname + " failed to load: " + std::string(err_msg));
        jl_exception_clear();
        throw PluginNotStarted("Julia plugin " + pluginname + " failed to load");
    }

    // Execute input phase
    PluginManager::getInstance().log("Executing input() For Julia Plugin " + pluginname);
    if (!callPluginFunction(moduleName, "input", inputname)) {
        // An unmet dependency() in input(): not started, as in every language.
        if (PluginManager::takeDependencyFailed())
            throw PluginNotStarted("Julia plugin " + pluginname + ": dependency not met");
        throw std::runtime_error("Julia plugin " + pluginname + " failed in input()");
    }

    // Execute run phase
    PluginManager::getInstance().log("Executing run() For Julia Plugin " + pluginname);
    if (!callPluginFunctionNoArgs(moduleName, "run")) {
        throw std::runtime_error("Julia plugin " + pluginname + " failed in run()");
    }

    // Execute output phase
    PluginManager::getInstance().log("Executing output() For Julia Plugin " + pluginname);
    if (!callPluginFunction(moduleName, "output", outputname)) {
        throw std::runtime_error("Julia plugin " + pluginname + " failed in output()");
    }

    PluginManager::getInstance().log("Julia Plugin " + pluginname + " completed successfully.");
#else
    (void) inputname;
    (void) outputname;
    notEnabled(pluginname);
#endif
}

void Julia::load() {
#ifdef HAVE_JULIA
    if (initialized) return;

    // Initialize Julia runtime
    jl_init();

    if (jl_exception_occurred()) {
        PluginManager::getInstance().log("Failed to initialize Julia runtime.");
        jl_exception_clear();
        return;
    }

    // Add plugin paths to Julia's LOAD_PATH
    std::vector<std::string> roots = pluma::splitPluginPath(pluginpath);
    for (const auto& root : roots) {
        if (root.empty()) continue;
        std::string pushCode = "push!(LOAD_PATH, \"" + root + "\")";
        jl_eval_string(pushCode.c_str());
    }

    initialized = true;
#endif
}

void Julia::unload() {
#ifdef HAVE_JULIA
    if (initialized) {
        // Clean up Julia runtime
        jl_atexit_hook(0);
        initialized = false;
    }
#endif
}

#ifdef HAVE_JULIA
std::string Julia::findJuliaPluginFile(const std::string& pluginname) const {
    // <plugin>/<plugin>Plugin.jl, else <plugin>/plugin.jl, root by root.
    std::vector<std::string> roots = pluma::splitPluginPath(pluginpath);
    for (const auto& root : roots) {
        if (root.empty()) continue;
        for (const std::string& name : { pluginname + "Plugin.jl", std::string("plugin.jl") }) {
            const std::string path = root + "/" + pluginname + "/" + name;
            if (pluma::fileExists(path)) return path;
        }
    }
    PluginManager::getInstance().log("Julia plugin " + pluginname + " not found in plugin path.");
    throw PluginNotStarted("Julia plugin " + pluginname + " not found");
}

bool Julia::callPluginFunction(const std::string& moduleName, const char* funcName,
                                const std::string& arg) {
    // Build the function call code
    std::string callCode = moduleName + "." + funcName + "(\"" + arg + "\")";

    jl_eval_string(callCode.c_str());

    if (jl_exception_occurred()) {
        jl_value_t* ex = jl_exception_occurred();
        const char* err_msg = jl_typeof_str(ex);
        PluginManager::getInstance().log(
            std::string("Julia plugin threw exception during ") + funcName + ": " + err_msg);
        jl_exception_clear();
        return false;
    }

    return true;
}

bool Julia::callPluginFunctionNoArgs(const std::string& moduleName, const char* funcName) {
    // Build the function call code
    std::string callCode = moduleName + "." + funcName + "()";

    jl_eval_string(callCode.c_str());

    if (jl_exception_occurred()) {
        jl_value_t* ex = jl_exception_occurred();
        const char* err_msg = jl_typeof_str(ex);
        PluginManager::getInstance().log(
            std::string("Julia plugin threw exception during ") + funcName + ": " + err_msg);
        jl_exception_clear();
        return false;
    }

    return true;
}
#endif
