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

#include <stdexcept>

#ifdef HAVE_PERL
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


#endif

Perl::Perl(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp)
{
    argc2 = 2;
    argv2 = new char*[2];
#ifdef HAVE_PERL
    my_perl = NULL;
    PERL_SYS_INIT3(&argc2, &argv2, &env);
#endif
}

Perl::~Perl()
{
    unload();
    if (argv2) delete[] argv2;
#ifdef HAVE_PERL
    PERL_SYS_TERM();
#endif
}

void Perl::load()
{
#ifdef HAVE_PERL
    if (my_perl) return;
    my_perl = perl_alloc();
    perl_construct(my_perl);
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    // Bootstrap the interpreter with a trivial no-op program. Individual
    // plugin scripts are then loaded into this persistent interpreter via
    // "do FILE" inside executePlugin(), the same way Py::executePlugin()
    // reuses a single Py_Initialize()'d interpreter across plugin calls
    // instead of tearing it down and rebuilding it every time.
    int bootargc = 3;
    char* bootargv[] = { (char*) "", (char*) "-e", (char*) "0", NULL };
    perl_parse(my_perl, xs_init, bootargc, bootargv, NULL);
#endif
}

void Perl::unload()
{
#ifdef HAVE_PERL
    if (my_perl) {
        perl_destruct(my_perl);
        perl_free(my_perl);
        my_perl = NULL;
    }
#endif
}

void Perl::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_PERL
    PluginManager::getInstance().log("Trying to run Perl plugin: "+pluginname+".");
    load(); // guarantees a persistent interpreter exists; no-op if already loaded
    if (!my_perl) {
        PluginManager::getInstance().log("Perl interpreter is not available; cannot run "+pluginname+".");
        return;
    }

    char *args_input[] = { (char*) inputname.c_str(), NULL };
    char *args_run[] = { NULL };
    char *args_output[] = { (char*) outputname.c_str(), NULL };
    std::string tmppath = pluginpath;
    std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));
    std::string filename;
    std::ifstream* infile = NULL;
    bool found = false;
    do {
        if (infile) delete infile;
        filename = path+"/"+pluginname+"/"+pluginname+"Plugin.pl";
        infile = new std::ifstream(filename.c_str(), std::ios::in);
        found = (bool)(*infile);
        tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
        path = tmppath.substr(0, tmppath.find_first_of(":"));
    } while (!found && path.length() > 0);
    delete infile;

    if (!found) {
        throw std::runtime_error("Perl plugin script not found for " + pluginname + " (looked for " + pluginname + "Plugin.pl on pluginpath).");
    }

    // Load THIS plugin's script into the persistent interpreter. Unlike
    // perl_parse() (which may only be run once per construct/destruct
    // cycle), "do FILE" can be evaluated repeatedly against the same
    // interpreter, mirroring how Py::executePlugin() re-imports each
    // plugin's module into its single persistent Python interpreter.
    // Modern Perl (>=5.26) no longer has "." in @INC, and "do FILE" falls
    // back to searching @INC for any path that isn't absolute or already
    // "./"/"../"-prefixed, so relative plugin paths must be normalized or
    // they silently fail to load.
    std::string doPath = (filename[0] == '/' || filename[0] == '.')
        ? filename
        : ("./" + filename);

    // Escape backslashes and double quotes so a pluginname/path containing
    // '"' or '\' cannot terminate the quoted string early and inject Perl
    // source into doExpr below.
    std::string escapedDoPath;
    escapedDoPath.reserve(doPath.length());
    for (char c : doPath) {
        if (c == '\\' || c == '"') escapedDoPath += '\\';
        escapedDoPath += c;
    }

    // Clear any input/run/output subs left over from the previously loaded
    // plugin so that a plugin script which fails to define one of them
    // errors out loudly (via call_argv failing) instead of silently
    // reusing the previous plugin's implementation. Best-effort: failure
    // here isn't itself checked, since the point is just to reset state.
    eval_pv("undef &main::input; undef &main::run; undef &main::output;", FALSE);

    // We already confirmed the file exists (the "found" check above), so
    // "do FILE" here can only fail via a genuine compile/runtime error,
    // which Perl reports by setting $@ -- NOT via its return value. A
    // successfully-loaded plugin script commonly returns a falsy value
    // (e.g. its last top-level statement is a "sub output { ... }"
    // declaration), so the return value of "do" must NOT be treated as a
    // pass/fail signal -- only $@ indicates a real error.
    std::string doExpr = "do \"" + escapedDoPath + "\";";
    // Do NOT croak_on_error (FALSE): a real Perl compile error would
    // otherwise be raised via croak() with no enclosing Perl eval frame at
    // this call site, hard-killing the process from inside eval_pv and
    // bypassing main.cxx's catch(...) fail-fast path (log, remove partial
    // output, exit(1)). Instead check $@ explicitly below and throw a C++
    // exception that main.cxx's existing catch(...) block already handles.
    eval_pv(doExpr.c_str(), FALSE);

    SV* errsv = get_sv("@", GV_ADD);
    if (SvTRUE(errsv)) {
        std::string errMsg = SvPV_nolen(errsv);
        throw std::runtime_error("Perl plugin load failed for " + pluginname + ": " + errMsg);
    }

    PluginManager::getInstance().log("Executing input() For Perl Plugin "+pluginname);
    call_argv("input", G_DISCARD, args_input);
    PluginManager::getInstance().log("Executing run() For Perl Plugin "+pluginname);
    call_argv("run", G_DISCARD | G_NOARGS, args_run);
    PluginManager::getInstance().log("Executing output() For Perl Plugin "+pluginname);
    call_argv("output", G_DISCARD, args_output);
    PluginManager::getInstance().log("Perl Plugin "+pluginname+" completed successfully.");
#endif
}
