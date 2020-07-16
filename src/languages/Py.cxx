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

#include "Py.h"
#include "../PluginManager.h"
#ifdef HAVE_PYTHON
#include "Python.h"
#endif

Py::Py(
    std::string language,
    std::string ext,
    std::string p
) : Language(language, ext, p) {}

void Py::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_PYTHON
    char* buffer = new char[100];
    std::string cwd = getcwd(buffer, 100);
    if (!Py_IsInitialized()) Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(("sys.path.append(\""+cwd+"/\")").c_str());

    std::string tmppath = pluginpath;
    std::string path = tmppath.substr(0, pluginpath.find_first_of(":"));

    while (path.length() > 0) {
        PyRun_SimpleString(("sys.path.append(\""+path+pluginname+"/\")").c_str());
        PyRun_SimpleString(("sys.path.append(\""+path+"/\")").c_str());
        tmppath = tmppath.substr(tmppath.find_first_of(":")+1, tmppath.length());
        path = tmppath.substr(0, tmppath.find_first_of(":"));
    }

    PyRun_SimpleString(("import "+pluginname+"Plugin").c_str());
    PyRun_SimpleString(("plugin = "+pluginname+"Plugin."+pluginname+"Plugin()").c_str());
    PluginManager::getInstance().log("[PluMA] Executing input() For Python Plugin "+pluginname);
    PyRun_SimpleString(("plugin.input(\""+inputname+"\")").c_str());
    PluginManager::getInstance().log("[PluMA] Executing run() For Python Plugin "+pluginname);
    PyRun_SimpleString("plugin.run()");
    PluginManager::getInstance().log("[PluMA] Executing output() For Python Plugin "+pluginname);
    PyRun_SimpleString(("plugin.output(\""+outputname+"\")").c_str());
    //Py_Finalize();
    PluginManager::getInstance().log("[PluMA] Python Plugin "+pluginname+" complted successfully.");
    delete buffer;
#endif
}

void Py::unload() {
#ifdef HAVE_PYTHON
    if (Py_IsInitialized()) Py_Finalize();
#endif
}
