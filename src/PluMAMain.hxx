#ifndef _PLUMA_MAIN_H
#define _PLUMA_MAIN_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include "cpp-subprocess/include/subprocess.hpp"

using namespace std;
namespace fs = std::filesystem;

class PluMA {
public:
    string pluginpath;

    PluMA();
    void read_config(char *inputfile, string prefix, bool doRestart, char *restartPoint);
    void search();
    void install();
private:
    string plugin_path;
    map<string, fs::path> *python_deps, *buildfiles;
    #if __linux__
    std::map<string, fs::path> *linux_deps;
    #elif __APPLE__
    map<string, fs::path> *macos_deps;
    #elif #elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
    map<string, fs::path> *windows_deps;
    #endif
    void _install(
        const string platform,
        const string type,
        const string command,
        vector<string> *args,
        map<string, fs::path> *installs
    ) {
        cout << "Installing " << platform << " " << type << "..." << endl;

        for (auto it = installs->cbegin(); it != installs->cend(); it++) {
            cout << it->first << ": " << type << endl;

            args->push_back(it->second.string());

            auto subprocess = subprocess::popen(command.c_str(), *args);

            cout << subprocess.stdout().rdbuf() << endl;

            subprocess.stderr().seekg(0, subprocess.stderr().end);

            size_t sz = subprocess.stderr().tellg();

            if (sz > 0) {
                cerr << subprocess.stderr().rdbuf() << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
};
#endif
