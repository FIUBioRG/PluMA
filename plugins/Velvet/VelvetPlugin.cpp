#include "PluginManager.h"
#include <stdio.h>
#include <stdlib.h>
#include "VelvetPlugin.h"


void VelvetPlugin::input(std::string file) {
	 inputfile = file;
 std::ifstream ifile(inputfile.c_str(), std::ios::in);

 std::string temp;
 ifile >> temp; // VELVETH
 ifile >> temp; // kmer length
 velvetH_parameters.push_back(temp);
 while (temp != "fastq") { // Parameters
	 ifile >> temp;
         velvetH_parameters.push_back("-"+temp);
 }
 while (temp != "VELVETG") {
    ifile >> temp;
    if (temp != "VELVETG")
    velvetH_parameters.push_back(std::string(PluginManager::prefix())+"/"+temp);
 }

 while (!ifile.eof()) {
   std::string key, value;
   ifile >> key;
   ifile >> value;
   velvetG_parameters[key] = value;
 }

}

void VelvetPlugin::run() {
   
}

void VelvetPlugin::output(std::string file) {
	 std::string outputfile = file;
 
 std::string myVelvetHCommand = "velveth "+outputfile+" ";
 std::string myVelvetGCommand = "velvetg "+outputfile+" ";
 for (int i = 0; i < velvetH_parameters.size(); i++)
	 myVelvetHCommand += velvetH_parameters[i] + " ";
 std::map<std::string, std::string>::iterator j;
 for (j = velvetG_parameters.begin(); j != velvetG_parameters.end(); j++) {
    if (j->first.size() != 0)
    myVelvetGCommand += "-" + j->first + " " + j->second + " ";

 }
 std::cout << myVelvetHCommand << std::endl;
 std::cout << myVelvetGCommand << std::endl;

 system(myVelvetHCommand.c_str());
 system(myVelvetGCommand.c_str());
 std::string copyCommand = "cp "+outputfile+"/stats.txt "+std::string(PluginManager::prefix());
 system(copyCommand.c_str());
}

PluginProxy<VelvetPlugin> VelvetPluginProxy = PluginProxy<VelvetPlugin>("Velvet", PluginManager::getInstance());
