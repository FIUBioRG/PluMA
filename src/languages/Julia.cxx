#include "Julia.h"
#include "../PluginManager.h"
#include <fstream>

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
    if (!initialized) load();
    if (!initialized) {
        PluginManager::getInstance().log("Julia runtime is not available; cannot run " + pluginname);
        return;
    }

    // Find the plugin file
    std::string pluginFile = findPluginFile(pluginname);
    if (pluginFile.empty()) {
        PluginManager::getInstance().log("Julia plugin " + pluginname + " not found in plugin path.");
        return;
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
        return;
    }

    // Execute input phase
    PluginManager::getInstance().log("Executing input() For Julia Plugin " + pluginname);
    if (!callPluginFunction(moduleName, "input", inputname)) {
        return;
    }

    // Execute run phase
    PluginManager::getInstance().log("Executing run() For Julia Plugin " + pluginname);
    if (!callPluginFunctionNoArgs(moduleName, "run")) {
        return;
    }

    // Execute output phase
    PluginManager::getInstance().log("Executing output() For Julia Plugin " + pluginname);
    if (!callPluginFunction(moduleName, "output", outputname)) {
        return;
    }

    PluginManager::getInstance().log("Julia Plugin " + pluginname + " completed successfully.");
#else
    PluginManager::getInstance().log("Julia support is not enabled in this build; skipping " + pluginname + ".");
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
    std::vector<std::string> roots = splitPaths(pluginpath);
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
std::vector<std::string> Julia::splitPaths(const std::string& paths) {
    std::vector<std::string> result;
    std::stringstream ss(paths);
    std::string entry;
    while (std::getline(ss, entry, ':')) {
        if (!entry.empty()) {
            result.push_back(entry);
        }
    }
    return result;
}

std::string Julia::findPluginFile(const std::string& pluginname) const {
    std::vector<std::string> roots = splitPaths(pluginpath);

    for (const auto& root : roots) {
        if (root.empty()) continue;

        // Check for PluginName/PluginNamePlugin.jl
        std::string path1 = root + "/" + pluginname + "/" + pluginname + "Plugin.jl";
        std::ifstream file1(path1);
        if (file1.good()) {
            return path1;
        }

        // Check for PluginName/plugin.jl
        std::string path2 = root + "/" + pluginname + "/plugin.jl";
        std::ifstream file2(path2);
        if (file2.good()) {
            return path2;
        }
    }

    return "";
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
