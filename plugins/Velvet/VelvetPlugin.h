#ifndef VELVETPLUGIN_H
#define VELVETPLUGIN_H

#include "Plugin.h"
#include "PluginProxy.h"
#include <string>
#include <vector>

class VelvetPlugin : public Plugin
{
public: 
 std::string toString() {return "Velvet";}
 void input(std::string file);
 void run();
 void output(std::string file);

private: 
 std::string inputfile;
 std::string outputfile;
 std::vector<std::string> velvetH_parameters;
 std::map<std::string, std::string> velvetG_parameters;
};

#endif
