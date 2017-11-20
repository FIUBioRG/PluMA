/********************************************************************************\

              Plugin-based Microbiome Analysis (PluMA, formerly MiAMi)

              Copyright (C) 2016 Bioinformatics Research Group (BioRG)
                        Florida International University

          This program is free software; you can redistribute it and/or
           modify it under the terms of the GNU General Public License
          as published by the Free Software Foundation; either version 3
              of the License, or (at your option) any later version.

         This program is distributed in the hope that it will be useful,
         but WITHOUT ANY WARRANTY; without even the implied warranty of
         MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                GNU General Public License for more details.

         You should have received a copy of the GNU General Public License
            along with this program; if not, write to the Free Software
           Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
                             02110-1335, USA.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\*********************************************************************************/



#include "R.h"
//#include "Python.h"
#include "PluginManager.h"

namespace MiAMi {

R::R(std::string language, std::string ext, std::string pp, int argc, char** argv) : Language(language, ext, pp) {
#ifdef HAVE_R
   myR = new RInside(argc, argv);
#endif
}


void R::executePlugin(std::string pluginname, std::string inputname, std::string outputname) {
#ifdef HAVE_R
        std::string tmppath = pluginpath;
        std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
        std::ifstream* infile = NULL;
        do {
           if (infile) delete infile;
           infile = new std::ifstream(path+"/"+pluginname+"/"+pluginname+"Plugin.R", std::ios::in);
           std::cout << "INFILE: " << path+"/"+pluginname+"/"+pluginname+"Plugin.R" << std::endl;
           tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
           path = tmppath.substr(0, tmppath.find_first_of(":"));
        } while (!(*infile) && path.length() > 0);// {

        
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

#endif

}


void R::unload() {
#ifdef HAVE_R
   delete myR;
#endif
}

}
