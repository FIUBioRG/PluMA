#ifndef INFOMAPPLUGIN_H
#define INFOMAPPLUGIN_H

#include "Plugin.h"
#include "PluginProxy.h"
#include <string>

class InfomapPlugin : public Plugin
{
public: 
 std::string toString() {return "Infomap";}
 void input(std::string file);
 void run();
 void output(std::string file);

private: 
 std::string inputfile;
 std::string outputfile;

};

#endif
