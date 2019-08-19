#include "PluginManager.h"
#include <stdio.h>
#include <stdlib.h>
#include "InfomapPlugin.h"

void InfomapPlugin::input(std::string file) {
 inputfile = file;
}

void InfomapPlugin::run() {}

void InfomapPlugin::output(std::string file) {
 std::string outputfile = file;
 std::string myCommand = "";
myCommand += "Infomap";
myCommand += " ";
myCommand += inputfile + " ";
myCommand += outputfile + " ";
 system(myCommand.c_str());
}

PluginProxy<InfomapPlugin> InfomapPluginProxy = PluginProxy<InfomapPlugin>("Infomap", PluginManager::getInstance());
