#include "Java.h"
#include "../PluginManager.h"
#include <fstream>

#ifdef HAVE_JAVA
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#endif

Java::Java(
    std::string language,
    std::string ext,
    std::string pp
) : Language(language, ext, pp)
#ifdef HAVE_JAVA
  , jvm(nullptr)
  , env(nullptr)
#endif
{}

Java::~Java()
{
    unload();
}

void Java::executePlugin(
    std::string pluginname,
    std::string inputname,
    std::string outputname
) {
#ifdef HAVE_JAVA
    if (!jvm) load();
    if (!env) {
        PluginManager::getInstance().log("Java VM is not available; cannot run " + pluginname);
        return;
    }

    std::vector<std::string> roots = splitPaths(pluginpath);
    std::string classFile;
    bool found = false;

    for (const auto& root : roots) {
        if (root.empty()) continue;
        classFile = root + "/" + pluginname + "/" + pluginname + "Plugin.class";
        std::ifstream infile(classFile.c_str(), std::ios::in);
        if (infile) {
            found = true;
            break;
        }
    }
    if (!found) {
        PluginManager::getInstance().log("Java plugin " + pluginname + " not found in plugin path.");
        return;
    }

    std::string className = pluginname + "Plugin";
    jclass pluginClass = env->FindClass(className.c_str());
    if (!pluginClass) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        PluginManager::getInstance().log("Java plugin class " + className + " could not be loaded.");
        return;
    }

    jmethodID constructor = env->GetMethodID(pluginClass, "<init>", "()V");
    if (!constructor) {
        PluginManager::getInstance().log("Java plugin " + pluginname + " is missing a default constructor.");
        env->DeleteLocalRef(pluginClass);
        return;
    }

    jobject pluginInstance = env->NewObject(pluginClass, constructor);
    if (!pluginInstance || env->ExceptionCheck()) {
        PluginManager::getInstance().log("Failed to instantiate Java plugin " + pluginname + ".");
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(pluginClass);
        return;
    }

    auto handleException = [&](const std::string& phase) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            PluginManager::getInstance().log("Java plugin " + pluginname + " threw an exception during " + phase + ".");
            return true;
        }
        return false;
    };

    auto callPhaseWithString = [&](const char* methodName, const std::string& value, const char* phase) {
        jmethodID method = env->GetMethodID(pluginClass, methodName, "(Ljava/lang/String;)V");
        if (!method) {
            PluginManager::getInstance().log("Java plugin " + pluginname + " missing method " + methodName);
            return false;
        }
        jstring arg = env->NewStringUTF(value.c_str());
        env->CallVoidMethod(pluginInstance, method, arg);
        env->DeleteLocalRef(arg);
        return !handleException(phase);
    };

    auto callPhaseNoArgs = [&](const char* methodName, const char* phase) {
        jmethodID method = env->GetMethodID(pluginClass, methodName, "()V");
        if (!method) {
            PluginManager::getInstance().log("Java plugin " + pluginname + " missing method " + methodName);
            return false;
        }
        env->CallVoidMethod(pluginInstance, method);
        return !handleException(phase);
    };

    PluginManager::getInstance().log("Executing input() For Java Plugin " + pluginname);
    if (!callPhaseWithString("input", inputname, "input")) {
        env->DeleteLocalRef(pluginInstance);
        env->DeleteLocalRef(pluginClass);
        return;
    }

    PluginManager::getInstance().log("Executing run() For Java Plugin " + pluginname);
    if (!callPhaseNoArgs("run", "run")) {
        env->DeleteLocalRef(pluginInstance);
        env->DeleteLocalRef(pluginClass);
        return;
    }

    PluginManager::getInstance().log("Executing output() For Java Plugin " + pluginname);
    if (!callPhaseWithString("output", outputname, "output")) {
        env->DeleteLocalRef(pluginInstance);
        env->DeleteLocalRef(pluginClass);
        return;
    }

    PluginManager::getInstance().log("Java Plugin " + pluginname + " completed successfully.");
    env->DeleteLocalRef(pluginInstance);
    env->DeleteLocalRef(pluginClass);
#else
    PluginManager::getInstance().log("Java support is not enabled in this build; skipping " + pluginname + ".");
#endif
}

void Java::load() {
#ifdef HAVE_JAVA
    if (jvm) return;
    std::vector<std::string> directories = collectClasspathDirs();
    std::string classpath;
    for (size_t i = 0; i < directories.size(); ++i) {
        if (i > 0) classpath += ":";
        classpath += directories[i];
    }

    optionStrings.clear();
    vmOptions.clear();
    optionStrings.push_back("-Djava.class.path=" + classpath);
    vmOptions.resize(optionStrings.size());
    for (size_t i = 0; i < vmOptions.size(); ++i) {
        vmOptions[i].optionString = const_cast<char*>(optionStrings[i].c_str());
        vmOptions[i].extraInfo = nullptr;
    }

    JavaVMInitArgs vm_args;
    vm_args.version = JNI_VERSION_1_8;
    vm_args.options = vmOptions.data();
    vm_args.nOptions = static_cast<jint>(vmOptions.size());
    vm_args.ignoreUnrecognized = JNI_FALSE;

    jint result = JNI_CreateJavaVM(&jvm, reinterpret_cast<void**>(&env), &vm_args);
    if (result != JNI_OK) {
        PluginManager::getInstance().log("Failed to start Java VM for plugin execution.");
        jvm = nullptr;
        env = nullptr;
    }
#endif
}

void Java::unload() {
#ifdef HAVE_JAVA
    if (jvm) {
        jvm->DestroyJavaVM();
        jvm = nullptr;
        env = nullptr;
    }
#endif
}

#ifdef HAVE_JAVA
std::vector<std::string> Java::splitPaths(const std::string& paths) {
    std::vector<std::string> result;
    std::stringstream ss(paths);
    std::string entry;
    while (std::getline(ss, entry, ':')) {
        if (!entry.empty()) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<std::string> Java::collectClasspathDirs() const {
    std::vector<std::string> classpath;
    classpath.push_back(".");
    std::vector<std::string> roots = splitPaths(pluginpath);
    for (const auto& root : roots) {
        if (root.empty()) continue;
        classpath.push_back(root);
        DIR* dir = opendir(root.c_str());
        if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string child = root + "/" + entry->d_name;
            struct stat buf;
            if (stat(child.c_str(), &buf) == 0 && S_ISDIR(buf.st_mode)) {
                classpath.push_back(child);
            }
        }
        closedir(dir);
    }
    return classpath;
}
#endif

