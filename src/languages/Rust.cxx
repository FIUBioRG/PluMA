/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018-2020 Bioinformatics Research Group (BioRG)
                       Florida International University


     Permission is hereby granted, free of charge, to any person obtaining
          a copy of this software and associated documentation files
        (the "Software"), to deal in the Software without restriction,
      including without limitation the rights to use, copy, modify, merge,
      publish, distribute, sublicense, and/or sell copies of the Software,
       and to permit persons to whom the Software is furnished to do so,
                    subject to the following conditions:

    The above copyright notice and this permission notice shall be included
            in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
           SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\*********************************************************************************/

#include "Rust.h"
#include "../PluginManager.h"
#include <iostream>
#include <fstream>

#ifdef HAVE_RUST
#include <dlfcn.h>
#endif

Rust::Rust(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp, "lib") {}

void Rust::loadPlugin(
    std::string path,
    glob_t* globbuf,
    std::map<std::string, std::string>* pluginLanguages,
    bool list
) {
#ifdef HAVE_RUST
    // Rust plugins are identified by the presence of Cargo.toml
    // and a compiled .so file (lib*Plugin.so)
    std::string pathGlob = path + "/*/Cargo.toml";

    if (glob(pathGlob.c_str(), 0, NULL, globbuf) == 0) {
        for (unsigned int i = 0; i < globbuf->gl_pathc; i++) {
            std::string cargoPath = globbuf->gl_pathv[i];

            // Extract plugin directory and name
            std::string pluginDir = cargoPath.substr(0, cargoPath.find_last_of("/"));
            std::string pluginName = pluginDir.substr(pluginDir.find_last_of("/") + 1);

            // Check if compiled .so exists
            std::string soPath = pluginDir + "/lib" + pluginName + "Plugin.so";
            std::ifstream soFile(soPath.c_str());

            if (soFile.good()) {
                // Register this as a Rust plugin
                std::string keyName = pluginName + "Plugin";
                (*pluginLanguages)[keyName] = language;

                if (list) {
                    std::cout << "Plugin: " << pluginName << " Language: " << language << " Path: " << pluginDir << std::endl;
                }
            } else {
                std::cout << "Warning: Rust plugin " << pluginName << " found but not compiled (missing " << soPath << ")" << std::endl;
            }
        }
    } else {
        std::cout << "Found no " << language << " plugins" << std::endl;
    }
#else
    std::cout << "Found no " << language << " plugins (Rust support not compiled)" << std::endl;
#endif
}

#ifdef HAVE_RUST
RustPluginVTable Rust::loadRustPlugin(const std::string& path, const std::string& pluginname) {
    RustPluginVTable vtable = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    // Open the shared library
    void* handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "[PluMA] Error loading Rust plugin " << pluginname << ": " << dlerror() << std::endl;
        return vtable;
    }

    vtable.handle = handle;

    // Load the FFI function symbols
    // These are the standard function names exported by pluma-plugin-trait
    std::string prefix = pluginname;

    vtable.create = (rust_plugin_create_fn)dlsym(handle, (prefix + "_plugin_create").c_str());
    if (!vtable.create) {
        // Try alternate naming convention
        vtable.create = (rust_plugin_create_fn)dlsym(handle, "plugin_create");
    }

    vtable.destroy = (rust_plugin_destroy_fn)dlsym(handle, (prefix + "_plugin_destroy").c_str());
    if (!vtable.destroy) {
        vtable.destroy = (rust_plugin_destroy_fn)dlsym(handle, "plugin_destroy");
    }

    vtable.input = (rust_plugin_input_fn)dlsym(handle, (prefix + "_plugin_input").c_str());
    if (!vtable.input) {
        vtable.input = (rust_plugin_input_fn)dlsym(handle, "plugin_input");
    }

    vtable.run = (rust_plugin_run_fn)dlsym(handle, (prefix + "_plugin_run").c_str());
    if (!vtable.run) {
        vtable.run = (rust_plugin_run_fn)dlsym(handle, "plugin_run");
    }

    vtable.output = (rust_plugin_output_fn)dlsym(handle, (prefix + "_plugin_output").c_str());
    if (!vtable.output) {
        vtable.output = (rust_plugin_output_fn)dlsym(handle, "plugin_output");
    }

    // Verify all required functions are loaded
    if (!vtable.create || !vtable.destroy || !vtable.input || !vtable.run || !vtable.output) {
        std::cerr << "[PluMA] Warning: Rust plugin " << pluginname << " missing required FFI functions" << std::endl;
        std::cerr << "  create: " << (vtable.create ? "found" : "MISSING") << std::endl;
        std::cerr << "  destroy: " << (vtable.destroy ? "found" : "MISSING") << std::endl;
        std::cerr << "  input: " << (vtable.input ? "found" : "MISSING") << std::endl;
        std::cerr << "  run: " << (vtable.run ? "found" : "MISSING") << std::endl;
        std::cerr << "  output: " << (vtable.output ? "found" : "MISSING") << std::endl;
    }

    return vtable;
}
#endif

void Rust::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname)
{
#ifdef HAVE_RUST
    std::string tmppath = pluginpath;
    std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
    std::ifstream* infile = nullptr;
    std::string filename;

    // Search for the Rust plugin shared library in plugin paths
    // Note: Rust plugins are compiled to .so files, not .rs
    do {
        if (infile) delete infile;
        // Rust plugins are compiled to lib<PluginName>Plugin.so
        filename = path + "/" + pluginname + "/lib" + pluginname + "Plugin.so";
        infile = new std::ifstream(filename.c_str(), std::ios::in);
        tmppath = tmppath.substr(tmppath.find_first_of(":") + 1, tmppath.length());
        path = tmppath.substr(0, tmppath.find_first_of(":"));
    } while (!(*infile) && path.length() > 0);

    delete infile;

    // Load the Rust plugin
    RustPluginVTable vtable = loadRustPlugin(filename, pluginname);

    if (!vtable.create || !vtable.input || !vtable.run || !vtable.output || !vtable.destroy) {
        PluginManager::getInstance().log("Error: Failed to load Rust plugin " + pluginname);
        return;
    }

    // Create plugin instance
    PluginManager::getInstance().log("Creating Rust Plugin " + pluginname);
    void* plugin_instance = vtable.create();

    if (!plugin_instance) {
        PluginManager::getInstance().log("Error: Failed to create Rust plugin instance for " + pluginname);
        return;
    }

    // Execute plugin phases
    PluginManager::getInstance().log("Executing input() For Rust Plugin " + pluginname);
    vtable.input(plugin_instance, inputname.c_str());

    PluginManager::getInstance().log("Executing run() For Rust Plugin " + pluginname);
    vtable.run(plugin_instance);

    PluginManager::getInstance().log("Executing output() For Rust Plugin " + pluginname);
    vtable.output(plugin_instance, outputname.c_str());

    // Cleanup
    PluginManager::getInstance().log("Destroying Rust Plugin " + pluginname);
    vtable.destroy(plugin_instance);

    PluginManager::getInstance().log("Rust Plugin " + pluginname + " completed successfully.");

    // Store vtable for potential cleanup
    loadedPlugins[pluginname] = vtable;
#endif
}

void Rust::unload() {
#ifdef HAVE_RUST
    // Close all loaded plugin handles
    for (auto& pair : loadedPlugins) {
        if (pair.second.handle) {
            dlclose(pair.second.handle);
            pair.second.handle = nullptr;
        }
    }
    loadedPlugins.clear();
#endif
}
