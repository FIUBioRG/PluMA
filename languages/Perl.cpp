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


#include "Perl.h"
//#include "Python.h"
#include "PluginManager.h"

#ifdef HAVE_PERL
#include <EXTERN.h>
#include <perl.h>
static PerlInterpreter *my_perl;
#endif

Perl::Perl(std::string language, std::string ext, std::string pp) : Language(language, ext, pp) {} 


void Perl::executePlugin(std::string pluginname, std::string inputname, std::string outputname) {
#ifdef HAVE_PERL
            PluginManager::getInstance().log("Trying to run Perl plugin: "+pluginname+"."); 
            char** env;
            int argc2 = 2;
            char *args_input[] = { (char*) inputname.c_str() };
            char *args_run[] = { NULL };
            char *args_output[] = { (char*) outputname.c_str() };
            char **argv2 = new char*[2];
            std::string tmppath = pluginpath;
            std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
            std::string filename;
            std::ifstream* infile = NULL;
            do {
              if (infile) delete infile;
              filename = path+"/"+pluginname+"/"+pluginname+"Plugin.pl";
              infile = new std::ifstream(filename, std::ios::in);
              tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
              path = tmppath.substr(0, tmppath.find_first_of(":"));
            } while (!(*infile) && path.length() > 0);// {
            delete infile;
            //argv2[1] = (char*) ("plugins/"+pluginname+"/"+pluginname+"Plugin.pl").c_str();
            argv2[1] = (char*) filename.c_str();

            PERL_SYS_INIT3(&argc2,&argv2,&env);
            my_perl = perl_alloc();
            perl_construct(my_perl);
            perl_parse(my_perl, NULL, argc2, argv2, NULL);
            PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
            /*** skipping perl_run() ***/
            PluginManager::getInstance().log("Executing input() For Perl Plugin "+pluginname);
            call_argv("input", G_DISCARD, args_input);
            PluginManager::getInstance().log("Executing run() For Perl Plugin "+pluginname);
            call_argv("run", G_DISCARD | G_NOARGS, args_run);
            PluginManager::getInstance().log("Executing output() For Perl Plugin "+pluginname);
            call_argv("output", G_DISCARD, args_output);
            perl_destruct(my_perl);
            perl_free(my_perl);
            PERL_SYS_TERM();
            PluginManager::getInstance().log("Perl Plugin "+pluginname+" completed successfully.");
#endif


}
