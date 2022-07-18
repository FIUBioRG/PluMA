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

#ifdef HAVE_R
#include "R.hxx"
#include "../PluginManager.h"
#include <vector>
#include <string>

namespace MiAMi {

    R::R(
        std::string language,
        std::string ext,
        std::string pp,
        int argc,
        char** argv
    ) : Language(language, ext, pp) {
        this->argc = argc;
        this->argv = argv;
        myR = new RInside(argc, argv);
    }

    void R::load() {
        myR = new RInside(argc, argv);
    }

    void R::executePlugin(
        std::string pluginname,
        std::string inputname,
        std::string outputname)
    {
        std::string tmppath = pluginpath;
        std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
        std::ifstream* infile = NULL;
        do {
            if (infile) delete infile;
            infile = new std::ifstream((path+"/"+pluginname+"/"+pluginname+"Plugin.R").c_str(), std::ios::in);
            tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
            path = tmppath.substr(0, tmppath.find_first_of(":"));
        } while (!(*infile) && path.length() > 0);

        std::string txt;
        std::string line;

        while (!infile->eof()) {
            getline(*infile, line);
            txt += line+"\n";
        }
        delete infile;

        PluginManager::getInstance().log("Executing R Plugin "+pluginname);
        txt += "input(\"" + inputname + "\");\n";
        txt += "run();";
        txt += "output(\"" + outputname + "\");\n";
        myR->parseEvalQ(txt);
        PluginManager::getInstance().log("R Plugin "+pluginname+" completed successfully.");
    }


    void R::unload() {
        delete myR;
        Rf_KillAllDevices();
        R_GlobalContext = NULL;
        Rf_endEmbeddedR(0);
    }

    void R::installDependencies(std::vector<std::string> dependencies) {
        for(auto &dependency: dependencies) {
            installDependency(dependency);
        }
    }

    void R::installDependency(std::string &dependency) {
        std::string command = "if(!(require(" + dependency;
        command += "))) install.packages(\"" + dependency + "\")";
        myR->parseEvalQ(command);
        PluginManager::getInstance().log("R dependency " + dependency + " installed");
    }
}

#endif
