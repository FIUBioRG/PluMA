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

\********************************************************************************/

#ifndef PLUMA_PLATFORM_H
#define PLUMA_PLATFORM_H

/**
 * @file platform.h
 * @brief Platform abstraction layer for cross-platform compatibility
 * 
 * This header provides platform-independent interfaces for:
 * - Dynamic library loading (dlopen/LoadLibrary)
 * - File path handling
 * - Directory globbing
 */

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define PLUMA_PLATFORM_WINDOWS 1
    #define PLUMA_PLATFORM_UNIX 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLUMA_PLATFORM_WINDOWS 0
    #define PLUMA_PLATFORM_UNIX 1
    #define PLUMA_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define PLUMA_PLATFORM_WINDOWS 0
    #define PLUMA_PLATFORM_UNIX 1
    #define PLUMA_PLATFORM_LINUX 1
#else
    #define PLUMA_PLATFORM_WINDOWS 0
    #define PLUMA_PLATFORM_UNIX 1
#endif

// =============================================================================
// Platform-Specific Includes
// =============================================================================

#if PLUMA_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    
    // For glob emulation
    #include <vector>
    #include <string>
#else
    #include <dlfcn.h>
    #include <glob.h>
    #include <unistd.h>
    #include <dirent.h>
#endif

#include <string>
#include <cstring>
#include <iostream>

// =============================================================================
// Shared Library Extension
// =============================================================================

#if PLUMA_PLATFORM_WINDOWS
    #define PLUMA_SHARED_LIB_EXT ".dll"
    #define PLUMA_SHARED_LIB_PREFIX ""
#elif defined(PLUMA_PLATFORM_MACOS)
    #define PLUMA_SHARED_LIB_EXT ".dylib"
    #define PLUMA_SHARED_LIB_PREFIX "lib"
#else
    #define PLUMA_SHARED_LIB_EXT ".so"
    #define PLUMA_SHARED_LIB_PREFIX "lib"
#endif

// =============================================================================
// Path Separator
// =============================================================================

#if PLUMA_PLATFORM_WINDOWS
    #define PLUMA_PATH_SEPARATOR "\\"
    #define PLUMA_PATH_LIST_SEPARATOR ";"
#else
    #define PLUMA_PATH_SEPARATOR "/"
    #define PLUMA_PATH_LIST_SEPARATOR ":"
#endif

// =============================================================================
// Dynamic Library Loading Abstraction
// =============================================================================

namespace pluma {
namespace platform {

/**
 * Handle type for dynamically loaded libraries
 */
#if PLUMA_PLATFORM_WINDOWS
    typedef HMODULE LibraryHandle;
#else
    typedef void* LibraryHandle;
#endif

/**
 * Load a dynamic library
 * @param path Path to the library file
 * @return Handle to the loaded library, or nullptr on failure
 */
inline LibraryHandle loadLibrary(const std::string& path) {
#if PLUMA_PLATFORM_WINDOWS
    return LoadLibraryA(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif
}

/**
 * Unload a dynamic library
 * @param handle Handle to the library to unload
 * @return true on success, false on failure
 */
inline bool unloadLibrary(LibraryHandle handle) {
    if (!handle) return false;
#if PLUMA_PLATFORM_WINDOWS
    return FreeLibrary(handle) != 0;
#else
    return dlclose(handle) == 0;
#endif
}

/**
 * Get a symbol from a loaded library
 * @param handle Handle to the library
 * @param name Name of the symbol to retrieve
 * @return Pointer to the symbol, or nullptr on failure
 */
inline void* getSymbol(LibraryHandle handle, const std::string& name) {
    if (!handle) return nullptr;
#if PLUMA_PLATFORM_WINDOWS
    return reinterpret_cast<void*>(GetProcAddress(handle, name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
}

/**
 * Get the last error message from library loading
 * @return Error message string
 */
inline std::string getLibraryError() {
#if PLUMA_PLATFORM_WINDOWS
    DWORD error = GetLastError();
    if (error == 0) return "";
    
    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer, 0, NULL
    );
    
    std::string message(buffer, size);
    LocalFree(buffer);
    return message;
#else
    const char* error = dlerror();
    return error ? std::string(error) : "";
#endif
}

// =============================================================================
// Glob/Directory Matching Abstraction
// =============================================================================

#if PLUMA_PLATFORM_WINDOWS

/**
 * Windows glob_t emulation structure
 */
struct glob_t {
    size_t gl_pathc;           // Count of paths matched
    char** gl_pathv;           // List of matched pathnames
    std::vector<std::string> _paths;  // Internal storage
    
    glob_t() : gl_pathc(0), gl_pathv(nullptr) {}
    
    ~glob_t() {
        if (gl_pathv) {
            for (size_t i = 0; i < gl_pathc; i++) {
                delete[] gl_pathv[i];
            }
            delete[] gl_pathv;
        }
    }
};

/**
 * Perform glob pattern matching on Windows
 * @param pattern The glob pattern (supports * and ?)
 * @param flags Unused on Windows (for API compatibility)
 * @param errfunc Unused on Windows
 * @param pglob Pointer to glob_t structure to fill
 * @return 0 on success, non-zero on failure
 */
inline int glob(const char* pattern, int flags, int (*errfunc)(const char*, int), glob_t* pglob) {
    (void)flags;
    (void)errfunc;
    
    pglob->_paths.clear();
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = nullptr;
        return -1;
    }
    
    // Extract directory from pattern
    std::string patternStr(pattern);
    std::string dir;
    size_t lastSlash = patternStr.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        dir = patternStr.substr(0, lastSlash + 1);
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            pglob->_paths.push_back(dir + findData.cFileName);
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    // Build gl_pathv from _paths
    pglob->gl_pathc = pglob->_paths.size();
    if (pglob->gl_pathc > 0) {
        pglob->gl_pathv = new char*[pglob->gl_pathc];
        for (size_t i = 0; i < pglob->gl_pathc; i++) {
            pglob->gl_pathv[i] = new char[pglob->_paths[i].length() + 1];
            strcpy(pglob->gl_pathv[i], pglob->_paths[i].c_str());
        }
    } else {
        pglob->gl_pathv = nullptr;
    }
    
    return 0;
}

/**
 * Free glob structure (no-op on Windows, destructor handles it)
 */
inline void globfree(glob_t* pglob) {
    // Destructor handles cleanup
    (void)pglob;
}

#endif // PLUMA_PLATFORM_WINDOWS

// =============================================================================
// File System Utilities
// =============================================================================

/**
 * Check if a file exists
 * @param path Path to the file
 * @return true if file exists, false otherwise
 */
inline bool fileExists(const std::string& path) {
#if PLUMA_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path.c_str(), F_OK) != -1;
#endif
}

/**
 * Check if a directory exists
 * @param path Path to the directory
 * @return true if directory exists, false otherwise
 */
inline bool directoryExists(const std::string& path) {
#if PLUMA_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path.c_str(), F_OK) != -1;
#endif
}

/**
 * Get the current working directory
 * @return Current working directory path
 */
inline std::string getCurrentDirectory() {
#if PLUMA_PLATFORM_WINDOWS
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return std::string(buffer);
#else
    char buffer[PATH_MAX];
    if (getcwd(buffer, PATH_MAX)) {
        return std::string(buffer);
    }
    return ".";
#endif
}

/**
 * Remove a file
 * @param path Path to the file to remove
 * @return true on success, false on failure
 */
inline bool removeFile(const std::string& path) {
#if PLUMA_PLATFORM_WINDOWS
    return DeleteFileA(path.c_str()) != 0;
#else
    return unlink(path.c_str()) == 0;
#endif
}

/**
 * Get environment variable value
 * @param name Name of the environment variable
 * @return Value of the variable, or empty string if not found
 */
inline std::string getEnvVar(const std::string& name) {
#if PLUMA_PLATFORM_WINDOWS
    char buffer[32767];
    DWORD result = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
    if (result > 0 && result < sizeof(buffer)) {
        return std::string(buffer);
    }
    return "";
#else
    const char* value = getenv(name.c_str());
    return value ? std::string(value) : "";
#endif
}

} // namespace platform
} // namespace pluma

// =============================================================================
// Convenience Macros for Conditional Compilation
// =============================================================================

#if PLUMA_PLATFORM_WINDOWS
    #define PLUMA_EXPORT __declspec(dllexport)
    #define PLUMA_IMPORT __declspec(dllimport)
#else
    #define PLUMA_EXPORT __attribute__((visibility("default")))
    #define PLUMA_IMPORT
#endif

#endif // PLUMA_PLATFORM_H
