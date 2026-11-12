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
    // Shuts the JVM down (shutdown hooks run, output is flushed) -- unless a
    // non-daemon Java thread a plugin left running (an executor never shut
    // down) is still alive, which DestroyJavaVM would wait for forever: then
    // it only flushes System.out/System.err and leaves the JVM, as earlier
    // versions always did.
    void unloadAtExit();
    // System.out / System.err flushed, if the JVM is running.
    void flushBeforeAbort();

private:
#ifdef HAVE_JAVA
    JavaVM* jvm;
    std::vector<std::string> optionStrings;
    std::vector<JavaVMOption> vmOptions;

    std::vector<std::string> collectClasspathDirs() const;
#endif
};

#endif

