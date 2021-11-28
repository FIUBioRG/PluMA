#pragma once

#ifndef _PLUMA_MAIN_H
#define _PLUMA_MAIN_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

class PluMA {
public:
    string pluginpath;

    PluMA();
    void read_config(char *inputfile, string prefix, bool doRestart, char *restartPoint);
    void search();
    void install_dependencies();
    void install_library();
    vector<string> split(string str, const string delim) {
        vector<string> tokens;
        size_t pos = 0;

        while ((pos = str.find(delim)) != string::npos) {
            tokens.push_back(str.substr(0, pos));
            str.erase(0, pos + delim.length());
        }

        return tokens;
    }

private:
    string plugin_path;
    map<string, fs::path> python_deps, buildfiles;
    #if __linux__
    std::map<string, fs::path> linux_deps;
    #elif __APPLE__
    map<string, fs::path> macos_deps;
    #elif #elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(_WIN64)
    map<string, fs::path> windows_deps;
    #endif
    void install(const string platform, const string type, const string command, vector<string> *args, const std::map<string, fs::path> *installs);
};
#endif
