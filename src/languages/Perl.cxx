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


#include "Perl.h"
#include "../PluginManager.h"
#include "../platform.h"

#if defined(HAVE_PERL) && defined(_WIN32)
// Windows: Strawberry Perl is built with GCC and its headers cannot be
// compiled by MSVC, so the Perl runner lives in pluma_perl.dll (built with
// Strawberry's g++; see bridges/PerlBridge.h and PerlRun.h), loaded on first
// use after perl5XY.dll (by absolute path: the Perl SCons built against,
// then PATH).
#include "../bridges/PerlBridge.h"
#ifdef PLUMA_HAVE_BUILD_PATHS
#include "pluma_build_paths.h"
#endif

static PlumaPerlRun perlRun = NULL;
static bool perlSearched = false;

static void hostLog(const char* message) { PluginManager::getInstance().log(message); }

static bool loadPerlRunner() {
    if (perlSearched) return perlRun != NULL;
    perlSearched = true;
#ifdef PLUMA_PERL_DLL
    std::vector<std::string> dirs;
#ifdef PLUMA_BUILD_PERL_BIN
    dirs.push_back(PLUMA_BUILD_PERL_BIN);
#endif
    const std::vector<std::string> pathDirs = pluma::absolutePathDirs();
    dirs.insert(dirs.end(), pathDirs.begin(), pathDirs.end());
    if (!pluma::preloadDll(PLUMA_PERL_DLL, dirs)) {
        PluginManager::getInstance().log("Perl (" PLUMA_PERL_DLL ") was not found; install Strawberry Perl or put "
                                         "its perl\\bin on PATH.");
        return false;
    }
    std::string why;
    HMODULE bridge = pluma::loadBridge("pluma_perl.dll", "pluma_perl_bridge_version", PLUMA_PERL_BRIDGE_VERSION, why);
    PlumaPerlRun run = bridge ? (PlumaPerlRun) GetProcAddress(bridge, "pluma_perl_run") : NULL;
    if (bridge && !run) why = pluma::bridgeMismatch("pluma_perl.dll");
    if (!run) {
        PluginManager::getInstance().log("The Perl runner " + why + ".");
        return false;
    }
    perlRun = run;
    return true;
#else
    PluginManager::getInstance().log("This build has no Perl runner.");
    return false;
#endif
}
#endif

#if defined(HAVE_PERL) && !defined(_WIN32)
#include <EXTERN.h>
#include <perl.h>

EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void xs_init(pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
    PERL_UNUSED_CONTEXT;

    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


static PerlInterpreter *my_perl;


#endif

Perl::Perl(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp)
{
    argc2 = 2;
    argv2 = new char*[2];
#if defined(HAVE_PERL) && !defined(_WIN32)
    PERL_SYS_INIT3(&argc2, &argv2, &env);
#endif
    //my_perl = perl_alloc();
    //perl_construct(my_perl);
}

Perl::~Perl()
{
    if (argv2) delete[] argv2;
#if defined(HAVE_PERL) && !defined(_WIN32)
    PERL_SYS_TERM();
#endif
}

void Perl::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#if defined(HAVE_PERL) && defined(_WIN32)
    PluginManager::getInstance().log("Trying to run Perl plugin: "+pluginname+".");
    if (!loadPerlRunner()) return;
    const std::string filename = pluma::findInPluginPath(pluginpath, pluginname + "/" + pluginname + "Plugin.pl");
    const PerlHost host = { hostLog };
    perlRun(&host, pluginname.c_str(), filename.c_str(), inputname.c_str(), outputname.c_str());
#elif defined(HAVE_PERL)
    //PerlInterpreter *my_perl;
    PluginManager::getInstance().log("Trying to run Perl plugin: "+pluginname+".");
    //char** env;
    //int argc2 = 2;
    char *args_input[] = { (char*) inputname.c_str(), NULL };
    //char* args_input[1];
    //args_input[0] = (char*) inputname.c_str();
    char *args_run[] = { NULL };
    char *args_output[] = { (char*) outputname.c_str(), NULL };
    //char **argv2 = new char*[2];
    std::string tmppath = pluginpath;
    std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
    std::string filename;
    std::ifstream* infile = NULL;
    do {
        if (infile) delete infile;
        filename = path+"/"+pluginname+"/"+pluginname+"Plugin.pl";
        infile = new std::ifstream(filename.c_str(), std::ios::in);
        tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
        path = tmppath.substr(0, tmppath.find_first_of(":"));
    } while (!(*infile) && path.length() > 0);// {
    delete infile;
    //argv2[1] = (char*) ("plugins/"+pluginname+"/"+pluginname+"Plugin.pl").c_str();
    argv2[0] = "";
    argv2[1] = (char*) filename.c_str();
    //printf("%s\n", args_input[0]);
    //PERL_SYS_INIT3(&argc2,&argv2,&env);
    my_perl = perl_alloc();
    perl_construct(my_perl);
    perl_parse(my_perl, xs_init, argc2, argv2, NULL);
    //perl_parse(my_perl, NULL, argc2, argv2, NULL);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    //eval_pv("use lib \'.\';", TRUE);
    /*** skipping perl_run() ***/
    PluginManager::getInstance().log("Executing input() For Perl Plugin "+pluginname);
    call_argv("input", G_DISCARD, args_input);
    PluginManager::getInstance().log("Executing run() For Perl Plugin "+pluginname);
    call_argv("run", G_DISCARD | G_NOARGS, args_run);
    PluginManager::getInstance().log("Executing output() For Perl Plugin "+pluginname);
    call_argv("output", G_DISCARD, args_output);
    perl_destruct(my_perl);
    perl_free(my_perl);
    //PERL_SYS_TERM();
    PluginManager::getInstance().log("Perl Plugin "+pluginname+" completed successfully.");
#endif
}
