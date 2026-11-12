#include "Java.h"
#include <stdexcept>
#include "../PluginManager.h"
#include "../platform.h"
#include <fstream>
#if defined(HAVE_JAVA) && defined(_WIN32) && defined(PLUMA_HAVE_BUILD_PATHS)
#include "pluma_build_paths.h"
#endif

#if defined(HAVE_JAVA) && defined(_WIN32)
// pluma.exe links jvm.dll with /DELAYLOAD so it can start on machines where
// the JVM is not on PATH.  Before the first JNI call, find jvm.dll and load
// it by full path; the delay-load helper then binds to the loaded module.

// JavaHome of the current JDK/JRE registered by the Oracle/OpenJDK installers.
static void registryJavaHomes(std::vector<std::string>& homes) {
    const char* roots[] = {
        "SOFTWARE\\JavaSoft\\JDK",
        "SOFTWARE\\JavaSoft\\Java Development Kit",
        "SOFTWARE\\JavaSoft\\JRE",
        "SOFTWARE\\JavaSoft\\Java Runtime Environment",
    };
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        std::string version = pluma::registryString(HKEY_LOCAL_MACHINE, roots[i], "CurrentVersion");
        if (version.empty()) continue;
        std::string home = pluma::registryString(HKEY_LOCAL_MACHINE,
                                                 std::string(roots[i]) + "\\" + version, "JavaHome");
        if (!home.empty()) homes.push_back(home);
    }
}

// jvm.dll candidates, in order of the user's explicit intent: JAVA_HOME,
// jvm.dll on PATH, the home of java.exe on PATH, the JavaSoft registry, and
// finally the JDK SCons compiled against.  PATH is searched through its
// absolute entries only (never the current directory).
static bool preloadJvm() {
    const char* subdirs[] = { "/bin/server", "/jre/bin/server", "/bin/client" };
    std::vector<std::string> dirs;
    auto addHome = [&](const std::string& home) {
        for (size_t j = 0; j < sizeof(subdirs) / sizeof(subdirs[0]); ++j)
            dirs.push_back(pluma::normalizeSlashes(home) + subdirs[j]);
    };

    if (const char* jh = getenv("JAVA_HOME")) addHome(jh);
    const std::vector<std::string> pathDirs = pluma::absolutePathDirs();
    dirs.insert(dirs.end(), pathDirs.begin(), pathDirs.end());
    // <home>\bin\java.exe -> <home>
    std::string javaExe = pluma::findInDirs(pathDirs, "java.exe");
    if (!javaExe.empty()) {
        addHome(pluma::dirName(pluma::dirName(pluma::finalPath(javaExe))));
    }
    std::vector<std::string> homes;
    registryJavaHomes(homes);
#ifdef PLUMA_BUILD_JAVA_HOME
    homes.push_back(PLUMA_BUILD_JAVA_HOME);   // the JDK SCons compiled against
#endif
    for (size_t i = 0; i < homes.size(); ++i) addHome(homes[i]);
    return pluma::preloadDll("jvm.dll", dirs);
}
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
        throw PluginNotStarted("Java VM is not available");
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
        throw PluginNotStarted("Java plugin " + pluginname + " not found");
    }

    std::string className = pluginname + "Plugin";
    jclass pluginClass = env->FindClass(className.c_str());
    if (!pluginClass) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        PluginManager::getInstance().log("Java plugin class " + className + " could not be loaded.");
        throw PluginNotStarted("Java plugin class " + className + " could not be loaded");
    }

    jmethodID constructor = env->GetMethodID(pluginClass, "<init>", "()V");
    if (!constructor) {
        env->ExceptionClear();
        PluginManager::getInstance().log("Java plugin " + pluginname + " is missing a default constructor.");
        env->DeleteLocalRef(pluginClass);
        throw PluginNotStarted("Java plugin " + pluginname + " is missing a default constructor");
    }

    jobject pluginInstance = env->NewObject(pluginClass, constructor);
    if (!pluginInstance || env->ExceptionCheck()) {
        PluginManager::getInstance().log("Failed to instantiate Java plugin " + pluginname + ".");
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(pluginClass);
        throw PluginNotStarted("Java plugin " + pluginname + " could not be instantiated");
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
            env->ExceptionClear();
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
            env->ExceptionClear();
            PluginManager::getInstance().log("Java plugin " + pluginname + " missing method " + methodName);
            return false;
        }
        env->CallVoidMethod(pluginInstance, method);
        return !handleException(phase);
    };

    // A failed phase: the plugin ran and failed (its output is removed) --
    // except a failing input(), which may have been an unmet dependency
    // (the step then counts as not started).
    auto fail = [&](const char* phase, bool notStarted) {
        env->DeleteLocalRef(pluginInstance);
        env->DeleteLocalRef(pluginClass);
        const std::string what = "Java plugin " + pluginname + " failed in " + phase + "()";
        if (notStarted) throw PluginNotStarted(what);
        throw std::runtime_error(what);
    };

    PluginManager::getInstance().log("Executing input() For Java Plugin " + pluginname);
    if (!callPhaseWithString("input", inputname, "input")) fail("input", PluginManager::takeDependencyFailed());

    PluginManager::getInstance().log("Executing run() For Java Plugin " + pluginname);
    if (!callPhaseNoArgs("run", "run")) fail("run", false);

    PluginManager::getInstance().log("Executing output() For Java Plugin " + pluginname);
    if (!callPhaseWithString("output", outputname, "output")) fail("output", false);

    PluginManager::getInstance().log("Java Plugin " + pluginname + " completed successfully.");
    env->DeleteLocalRef(pluginInstance);
    env->DeleteLocalRef(pluginClass);
#else
    notEnabled(pluginname);
#endif
}

void Java::load() {
#ifdef HAVE_JAVA
    if (jvm) return;
#ifdef _WIN32
    if (!preloadJvm()) {
        PluginManager::getInstance().log("Could not locate jvm.dll (set JAVA_HOME or put java.exe on PATH).");
        return;
    }
#endif
    std::vector<std::string> directories = collectClasspathDirs();
    std::string classpath;
    for (size_t i = 0; i < directories.size(); ++i) {
        if (i > 0) classpath += pluma::PATH_LIST_SEP;
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
    return pluma::splitPathList(paths);
}

std::vector<std::string> Java::collectClasspathDirs() const {
    std::vector<std::string> classpath;
    classpath.push_back(".");
    std::vector<std::string> roots = splitPaths(pluginpath);
    for (const auto& root : roots) {
        if (root.empty()) continue;
        classpath.push_back(root);
        std::vector<std::string> children = pluma::listSubdirectories(root);
        classpath.insert(classpath.end(), children.begin(), children.end());
    }
    return classpath;
}
#endif

