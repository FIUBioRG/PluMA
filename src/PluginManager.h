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

               Note: This particular file adapted from BasicUtils
               (C) 2003 Joseph Coffland under license from GNU GPL
\*********************************************************************************/

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "PluginMaker.h"

#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>

#include "languages/Compiled.h"
#include "languages/Py.h"
#include "languages/R.h"
#include "languages/Perl.h"

class PluginManager {
public:
    std::map<std::string, Maker*> makers;
    std::set<std::string> installed;
    static std::vector<Language*> supported;
    std::map<std::string, std::string> pluginLanguages;
    static std::string myPrefix;

    static PluginManager& getInstance()
    {
        static PluginManager instance; // Guaranteed to be destroyed.
                                       // Instantiated on first use.
        return instance;
    }

    PluginManager() {}
    PluginManager(PluginManager const&) = delete;
    ~PluginManager() {
        if (logfile) delete logfile;
    }
    void operator=(PluginManager const&) = delete;

public:
    template<class T> void addMaker(std::string name, PluginMaker<T>* maker) {
        makers[name] = maker;
    }

    void add(std::string name) {installed.insert(name);}

    Plugin* create(std::string name) {
        return makers[name]->create();
    }

    void setLogFile(std::string lf) {
        if (logfile) delete logfile;
        logfile = new std::ofstream(lf.c_str(), std::ios::out);
    }

    static void log(std::string msg) {
        *(getInstance().logfile) << "[PluMA] " << msg << std::endl;
    }

    static void dependency(std::string plugin) {
       if (getInstance().installed.count(plugin) == 0) {
           *(getInstance().logfile) << "[PluMA] Plugin dependency " << plugin << " not met.  Exiting..." << std::endl;  exit(1);
        } else {
            *(getInstance().logfile) << "[PluMA] Plugin dependency " << plugin << " met." << std::endl;
        }
    }

    static char* prefix() {
        return (char*) myPrefix.c_str();
    }

    static std::string addPrefix(std::string filename) {
        return std::string(prefix())+"/"+filename;
    }

    static void supportedLanguages(
        std::string pluginpath,
        int argc,
        char** argv)
    {
#ifdef APPLE
        supported.push_back(new Compiled("C", "dylib", pluginpath, "lib"));
#else
        supported.push_back(new Compiled("C", "so", pluginpath, "lib"));
#endif
        supported.push_back(new Py("Python", "py", pluginpath));
        supported.push_back(new MiAMi::R("R", "R", pluginpath, argc, argv));
        supported.push_back(new Perl("Perl", "pl", pluginpath));
    }

    static void languageLoad(std::string lang) {
        for (int i = 0; i < supported.size(); i++) {
            if (supported[i]->lang() == lang) {
                supported[i]->load();
            }
        }
    }

    static void languageUnload(std::string lang) {
        for (int i = 0; i < supported.size(); i++) {
            if (supported[i]->lang() == lang) {
                supported[i]->unload();
            }
        }
    }

private:
    std::ofstream* logfile;
};

#endif
