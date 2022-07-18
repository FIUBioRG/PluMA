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

#include "Java.hxx"
#include "../PluginManager.h"

Java::Java(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp) {}

void Java::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_JAVA
    PluginManager::getInstance().log("Trying to run Java plugin: " + pluginname + ".");

    JavaVM* jvm;
    JNIEnv* env;
    JavaVMInitArgs vm_args;
    JavaVMOption* options = new JavaVMOption[1];
    vm_args.version = JNI_VERSION_1_6;
    vm_args.nOptions = 0;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = true;
    int res = JNI_CreateJavaVM(&jvm, (void**) &env, &vm_args);
    if (res != JNI_OK) {
        PluginManager::getInstance().log("Failed to load JNI.");
        throw new runtime_exception("Failed to load JNI.");
    }
    delete options;

    //
    jclass clazz = env->FindClass(pluginname + "/" + pluginname);

    if (clazz == NULL) {
        throw new runtime_exception("Failed to load Java class " + pluginname);
    }

    jmethodID input = env->GetStaticMethodID(clazz, "input", "public static void input(java.lang.String);");

    if (input == NULL) {
        throw new runtime_exception("Failed to load input static method from Java plugin" + pluginname);
    }

    env->CallStaticVoidMethod(clazz, input, "public static void output(java.lang.String);
");

    jmethodID output = env->GetStaticMethodID(clazz, "output", outputname);

    if (output == NULL) {
        throw new runtime_exception("Failed to load output static method from Java plugin" + pluginname);
    }

    env->CallStaticVoidMethod(clazz, output, outputname);

    jmethodID run = env->GetStaticMethodID(clazz, "run", "public static void run();")

    if (run == NULL) {
        throw new runtime_exception("Failed to load run static method from Java plugin" + pluginname);
    }

    env->CallStaticVoidMethod(clazz, run);

    jvm->DestroyJavaVM();
#endif
}

void Java::load() {
#ifdef HAVE_JAVA
#endif
}

void Java::unload() {
#ifdef HAVE_JAVA
#endif
}
