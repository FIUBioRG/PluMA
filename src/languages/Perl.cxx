/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018 Bioinformatics Research Group (BioRG)
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

#include "Perl.h"
#include "PluginManager.h"

#ifdef HAVE_PERL
#include <EXTERN.h>
#include <perl.h>

EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void xs_init(pTHX) {
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;
    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


static PerlInterpreter *my_perl;

Perl::Perl(std::string language, std::string ext, std::string pp) :
    Language(language, ext, pp)
{
    argc2 = 2;
    argv2 = new char*[2];
    PERL_SYS_INIT3(&argc2, &argv2, &env);
}

Perl::~Perl() {
    if (argv2) delete[] argv2;
    PERL_SYS_TERM();
}

void Perl::executePlugin(std::string pluginname, std::string inputname,
    std::string outputname)
{
    PluginManager::getInstance().log("Trying to run Perl plugin: "
        +pluginname+".");
    char *args_input[] = { (char*) inputname.c_str(), NULL };
    char *args_run[] = { NULL };
    char *args_output[] = { (char*) outputname.c_str(), NULL };
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
    }
    while (!(*infile) && path.length() > 0);
    delete infile;
    argv2[0] = "";
    argv2[1] = (char*) filename.c_str();
    my_perl = perl_alloc();
    perl_construct(my_perl);
    perl_parse(my_perl, xs_init, argc2, argv2, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    PluginManager::getInstance().log("Executing input() For Perl Plugin "
        +pluginname);
    call_argv("input", G_DISCARD, args_input);
    PluginManager::getInstance().log("Executing run() For Perl Plugin "
        +pluginname);
    call_argv("run", G_DISCARD | G_NOARGS, args_run);
    PluginManager::getInstance().log("Executing output() For Perl Plugin  "
        +pluginname);
    call_argv("output", G_DISCARD, args_output);
    perl_destruct(my_perl);
    perl_free(my_perl);
    //PERL_SYS_TERM();
    PluginManager::getInstance().log("Perl Plugin "+pluginname
        +" completed successfully.");
}
#endif
