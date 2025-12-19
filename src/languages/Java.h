#ifndef JAVA_H
#define JAVA_H

#include "Language.h"

#ifdef HAVE_JAVA
#include <jni.h>
#include <vector>
#include <string>
#endif

class Java : public Language {
public:
    Java(std::string language, std::string ext, std::string pp);
    ~Java();
    void executePlugin(std::string pluginname, std::string inputname, std::string outputname);
    void load();
    void unload();

private:
#ifdef HAVE_JAVA
    JavaVM* jvm;
    JNIEnv* env;
    std::vector<std::string> optionStrings;
    std::vector<JavaVMOption> vmOptions;

    std::vector<std::string> collectClasspathDirs() const;
    static std::vector<std::string> splitPaths(const std::string& paths);
#endif
};

#endif

