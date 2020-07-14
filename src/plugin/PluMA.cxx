#include "PluMA.h"
#include "PluginManager.h"
#include <string>
using std::string;

void log(char* msg) {
    PluginManager::log(std::string(msg));
}

void dependency(char* plugin) {
    PluginManager::dependency(std::string(plugin));
}

char* prefix() {
    return PluginManager::prefix();
}

void languageLoad(char* lang) {
    PluginManager::languageLoad(std::string(lang));
}

void languageUnload(char* lang) {
    PluginManager::languageUnload(std::string(lang));
}
