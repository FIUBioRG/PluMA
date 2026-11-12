// Copyright (C) 2026 FIU BioRG
// SPDX-License-Identifier: MIT
//
// Thin portability layer over the handful of operating-system services PluMA
// needs: loading shared libraries, walking plugin directories, and basic path
// handling.  POSIX builds keep using glob()/dlopen() exactly as before; Windows
// builds use the Win32 API.
//
// Include this from .cxx files only: on Windows it pulls in <windows.h>, which
// should not leak into plugin code through the public headers.

#ifndef PLUMA_PLATFORM_H
#define PLUMA_PLATFORM_H

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <direct.h>
#  include <string.h>
#else
#  include <dlfcn.h>
#  include <glob.h>
#  include <dirent.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <stdlib.h>
#endif

// Platform macros (from the earlier src/platform.h; kept for code that tests
// them).  PluMA's own sources use the pluma:: helpers below.
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#  ifndef PLUMA_PLATFORM_WINDOWS
#    define PLUMA_PLATFORM_WINDOWS 1
#  endif
#  define PLUMA_PLATFORM_UNIX 0
#elif defined(__APPLE__) && defined(__MACH__)
#  define PLUMA_PLATFORM_WINDOWS 0
#  define PLUMA_PLATFORM_UNIX 1
#  define PLUMA_PLATFORM_MACOS 1
#elif defined(__linux__)
#  define PLUMA_PLATFORM_WINDOWS 0
#  define PLUMA_PLATFORM_UNIX 1
#  define PLUMA_PLATFORM_LINUX 1
#else
#  define PLUMA_PLATFORM_WINDOWS 0
#  define PLUMA_PLATFORM_UNIX 1
#endif

#if PLUMA_PLATFORM_WINDOWS
#  define PLUMA_SHARED_LIB_EXT ".dll"
#  define PLUMA_SHARED_LIB_PREFIX ""
#  define PLUMA_PATH_SEPARATOR "\\"
#  define PLUMA_PATH_LIST_SEPARATOR ";"
#  define PLUMA_EXPORT __declspec(dllexport)
#  define PLUMA_IMPORT __declspec(dllimport)
#else
#  if defined(PLUMA_PLATFORM_MACOS)
#    define PLUMA_SHARED_LIB_EXT ".dylib"
#  else
#    define PLUMA_SHARED_LIB_EXT ".so"
#  endif
#  define PLUMA_SHARED_LIB_PREFIX "lib"
#  define PLUMA_PATH_SEPARATOR "/"
#  define PLUMA_PATH_LIST_SEPARATOR ":"
#  define PLUMA_EXPORT __attribute__((visibility("default")))
#  define PLUMA_IMPORT
#endif

namespace pluma {

#ifdef _WIN32
// ':' cannot separate search-path entries on Windows because of drive letters.
const char PATH_LIST_SEP = ';';
// ':' is not allowed in Windows file names (used in log file timestamps).
const char TIMESTAMP_SEP = '-';
#else
const char PATH_LIST_SEP = ':';
const char TIMESTAMP_SEP = ':';
#endif

// Split a search path into its non-empty entries.
inline std::vector<std::string> splitPathList(const std::string& paths, char separator = PATH_LIST_SEP) {
    std::vector<std::string> result;
    std::string::size_type start = 0;
    while (start <= paths.size()) {
        std::string::size_type end = paths.find(separator, start);
        if (end == std::string::npos) end = paths.size();
        if (end > start) result.push_back(paths.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

// True if `name` ends with "Plugin.<ext>".  "Plugin" must match exactly on
// every platform (it is part of the name pipelines refer to); the extension
// is matched case-insensitively on Windows (FooPlugin.DLL), exactly elsewhere.
inline bool isPluginFileName(const std::string& name, const std::string& ext) {
    const std::string stem = "Plugin.";
    if (name.size() < stem.size() + ext.size()) return false;
    const std::string::size_type extPos = name.size() - ext.size();
    if (name.compare(extPos - stem.size(), stem.size(), stem) != 0) return false;
#ifdef _WIN32
    return _stricmp(name.c_str() + extPos, ext.c_str()) == 0;
#else
    return name.compare(extPos, ext.size(), ext) == 0;
#endif
}

// Convert backslashes to forward slashes on Windows so paths can be joined
// and compared with "/".  (Paths are handed to Python/R as values, never
// pasted into interpreter source.)  No-op on POSIX, where a backslash is a
// legal file-name character.
inline std::string normalizeSlashes(std::string path) {
#ifdef _WIN32
    std::replace(path.begin(), path.end(), '\\', '/');
#endif
    return path;
}

// The inverse, for Windows APIs and programs that need native separators
// (LoadLibraryEx with LOAD_WITH_ALTERED_SEARCH_PATH, sys.executable).
inline std::string nativeSlashes(std::string path) {
#ifdef _WIN32
    std::replace(path.begin(), path.end(), '/', '\\');
#endif
    return path;
}

// ASCII lower case (case-insensitive file and DLL names; locale-independent).
inline std::string toLowerAscii(std::string s) {
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = static_cast<char>(s[i] - 'A' + 'a');
    return s;
}

// A path relative to the current directory with nothing else in front:
// not absolute, and on Windows neither drive-relative ("D:data.txt", relative
// to drive D's current directory) nor rooted ("\data.txt") -- the only kind a
// pipeline Prefix can go in front of.
inline bool isAbsolutePath(const std::string& path);
inline bool isPlainRelativePath(const std::string& path) {
    if (path.empty()) return true;   // (as before: the prefix itself)
    if (isAbsolutePath(path)) return false;
#ifdef _WIN32
    if (path[0] == '\\' || (path.size() >= 2 && path[1] == ':')) return false;
#endif
    return true;
}

inline bool isAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/') return true;
#ifdef _WIN32
    if (path[0] == '\\') return true;   // \dir and \\server\share
    // "C:\..." or "C:/..." only: "C:foo" is relative to drive C's current
    // directory and must not count as absolute.
    if (path.size() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/') &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        return true;
#endif
    return false;
}

#ifdef _WIN32
// GetFullPathNameA (the current drive and directory applied, "." / ".."
// resolved), with native separators kept; false if it fails.
inline bool fullPathName(const std::string& path, std::string& out) {
    const DWORD len = GetFullPathNameA(path.c_str(), 0, NULL, NULL);
    if (len == 0) return false;
    std::vector<char> buf(len);
    const DWORD got = GetFullPathNameA(path.c_str(), len, buf.data(), NULL);
    if (got == 0 || got >= len) return false;
    out.assign(buf.data(), got);
    return true;
}

// The Win32 device namespace (\\.\pipe\x, \\.\COM1, \\.\NUL): never a file,
// and opening a pipe there would take one of its server's instances.
inline bool isDeviceNamespacePath(const std::string& path) {
    return path.size() >= 4 && (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/') &&
           path[2] == '.' && (path[3] == '\\' || path[3] == '/');
}
#endif

// `path` made absolute against the current directory (`cwd`, as returned by
// currentDirectory()).  Windows: resolved by the system, so drive-relative
// ("D:plugins": drive D's current directory) and root-relative ("\plugins":
// the current drive) forms are pinned down too; UNC paths are kept as given.
inline std::string absolutePath(const std::string& path, const std::string& cwd) {
#ifdef _WIN32
    const bool unc = path.size() >= 2 && (path[0] == '\\' || path[0] == '/') &&
                     (path[1] == '\\' || path[1] == '/');
    std::string full;
    if (!path.empty() && !unc) {
        // A plain relative path ("data/x.txt") is resolved against `cwd`, as
        // on POSIX; drive-relative ("D:x") and rooted ("\\x") ones need the
        // process's per-drive state, which only GetFullPathName has.
        const bool plainRelative = isPlainRelativePath(path);
        if (fullPathName(plainRelative && !cwd.empty() ? cwd + "/" + path : path, full)) return normalizeSlashes(full);
    }
#endif
    return isAbsolutePath(path) ? path : cwd + "/" + path;
}

inline std::string baseName(const std::string& path) {
#ifdef _WIN32
    std::string::size_type pos = path.find_last_of("/\\");
#else
    std::string::size_type pos = path.find_last_of('/');
#endif
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Directory part of a path ("a/b/c.txt" -> "a/b"); "." if there is none.
inline std::string dirName(const std::string& path) {
#ifdef _WIN32
    std::string::size_type pos = path.find_last_of("/\\");
#else
    std::string::size_type pos = path.find_last_of('/');
#endif
    if (pos == std::string::npos) return ".";
    if (pos == 0) return path.substr(0, 1);   // "/file" -> "/"
    return path.substr(0, pos);
}

inline std::string currentDirectory() {
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(0, NULL);
    if (len == 0) return ".";
    std::vector<char> buf(len);
    if (GetCurrentDirectoryA(len, buf.data()) == 0) return ".";
    return normalizeSlashes(std::string(buf.data()));
#else
    std::vector<char> buf(4096);
    while (getcwd(buf.data(), buf.size()) == NULL) {
        // Only a too-small buffer is worth retrying; a removed or unreadable
        // directory (ENOENT, EACCES) stays relative, as paths always were.
        if (errno != ERANGE || buf.size() > (1u << 20)) return ".";
        buf.resize(buf.size() * 2);
    }
    return std::string(buf.data());
#endif
}

inline bool fileExists(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path.c_str(), F_OK) != -1;
#endif
}

// Reads `length` bytes at `offset` of `file`; false if they are not all
// there.  (For the binary-format readers: only the parts needed are read.)
inline bool readFileAt(std::ifstream& file, unsigned long long offset, void* to, unsigned long long length) {
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(static_cast<char*>(to), static_cast<std::streamsize>(length));
    return file && static_cast<unsigned long long>(file.gcount()) == length;
}


// Removes a leading UTF-8 byte order mark (files saved by some Windows editors).
inline void stripUtf8Bom(std::string& text) {
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
}

// Reads all of `in` into `data`; false on a read error (not the end).
inline bool readWholeStream(std::istream& in, std::string& data) {
    char block[65536];
    while (in.read(block, sizeof(block)) || in.gcount() > 0) data.append(block, static_cast<size_t>(in.gcount()));
    return !in.bad();
}

inline bool isDirectory(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

#ifdef _WIN32
// Resolves symlinks, junctions and subst drives (e.g. Oracle's
// ...\Oracle\Java\javapath\java.exe); `path` itself if that fails.
inline std::string finalPath(const std::string& path) {
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return path;
    char buf[MAX_PATH * 4];
    DWORD n = GetFinalPathNameByHandleA(h, buf, sizeof(buf), FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (n == 0 || n >= sizeof(buf)) return path;
    std::string result(buf, n);
    if (result.compare(0, 8, "\\\\?\\UNC\\") == 0) result = "\\\\" + result.substr(8);
    else if (result.compare(0, 4, "\\\\?\\") == 0) result = result.substr(4);
    return result;
}
#endif

// Whether `path` is an ordinary file -- not a directory, device (/dev/null,
// NUL, CON), FIFO or socket.  Only such files are stamped (reading a FIFO or
// a terminal would block) or removed after a failure.
inline bool isRegularFile(const std::string& path) {
#ifdef _WIN32
    if (isDeviceNamespacePath(path)) return false;
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info;
    const bool regular = GetFileType(h) == FILE_TYPE_DISK && GetFileInformationByHandle(h, &info) &&
                         !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CloseHandle(h);
    return regular;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// What a pipeline step's output looked like, to tell whether the step touched
// it before failing: existence, size, modification time and change time (POSIX
// inode change time / NTFS ChangeTime, which every write updates and which,
// unlike the modification time, utime() cannot set back).  A file whose change
// time is more than kStampRecentSeconds old cannot be rewritten without its
// change time moving on (for any timestamp granularity up to that), so its
// content is not read.  Only when the change time cannot tell -- unavailable
// (FAT) or recent (a rewrite could fall in the same timestamp tick) -- is the
// content (files up to kStampHashLimit) compared too.  Anything but a regular
// file (a device, FIFO, directory) is only checked for existence: never read.
const unsigned long long kStampHashLimit = 16ULL * 1024 * 1024;
const long long kStampRecentSeconds = 2;

struct FileStamp {
    bool exists;
    bool regular;
    unsigned long long size;
    long long mtime;      // seconds (POSIX) or FILETIME ticks (Windows)
    long long mtimeNs;    // sub-second part on POSIX; 0 on Windows
    long long ctime;      // change time: seconds (POSIX) or FILETIME ticks (Windows; 0 = none)
    long long ctimeNs;
    bool hashed;
    unsigned long long hash;
    // Stamps to compare must be taken with the same hashing (see
    // fileStampLike): a content hash is compared only when both have one.
    bool operator==(const FileStamp& o) const {
        if (exists != o.exists || regular != o.regular) return false;
        if (!exists || !regular) return true;
        return size == o.size && mtime == o.mtime && mtimeNs == o.mtimeNs &&
               ctime == o.ctime && ctimeNs == o.ctimeNs && (!hashed || !o.hashed || hash == o.hash);
    }
    bool operator!=(const FileStamp& o) const { return !(*this == o); }
};

// 64-bit FNV-1a of the file's content, in `h`; false if it cannot be read
// (e.g. open in another step without read sharing).
inline bool fileContentHash(const std::string& path, unsigned long long& h) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in.is_open()) return false;
    h = 1469598103934665603ULL;
    char buf[65536];
    while (in) {
        in.read(buf, sizeof(buf));
        const std::streamsize n = in.gcount();
        for (std::streamsize i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 1099511628211ULL;
        }
    }
    return !in.bad();
}

enum StampHashing { STAMP_HASH_IF_NEEDED, STAMP_HASH, STAMP_NO_HASH };

inline FileStamp fileStamp(const std::string& path, StampHashing hashing = STAMP_HASH_IF_NEEDED) {
    FileStamp s = { false, false, 0, 0, 0, 0, 0, false, 0 };
    bool changeTimeReliable = false;
#ifdef _WIN32
    if (isDeviceNamespacePath(path)) {   // never a regular file; never opened
        s.exists = fileExists(path);
        return s;
    }
    // One open for everything (this runs before every step).
    HANDLE h = CreateFileA(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        s.exists = fileExists(path);   // e.g. present but not openable
        return s;
    }
    s.exists = true;
    FILE_BASIC_INFO basic;
    FILE_STANDARD_INFO standard;
    const bool ok = GetFileType(h) == FILE_TYPE_DISK &&
                    GetFileInformationByHandleEx(h, FileBasicInfo, &basic, sizeof(basic)) &&
                    GetFileInformationByHandleEx(h, FileStandardInfo, &standard, sizeof(standard)) &&
                    !standard.Directory;
    CloseHandle(h);
    if (!ok) return s;
    s.regular = true;
    s.size = static_cast<unsigned long long>(standard.EndOfFile.QuadPart);
    s.mtime = basic.LastWriteTime.QuadPart;
    s.ctime = basic.ChangeTime.QuadPart;
    const long long ticksPerSecond = 10000000LL;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const long long nowTicks = static_cast<long long>((static_cast<unsigned long long>(now.dwHighDateTime) << 32) |
                                                      now.dwLowDateTime);
    // A volume with no change time (FAT, exFAT, some network shares: 0) goes
    // by the modification time, recorded in 2-second steps on FAT.
    const long long changed = s.ctime != 0 ? s.ctime : s.mtime;
    const long long margin = s.ctime != 0 ? kStampRecentSeconds : kStampRecentSeconds + 2;
    changeTimeReliable = changed != 0 && nowTicks - changed > margin * ticksPerSecond;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        s.exists = fileExists(path);
        return s;
    }
    s.exists = true;
    if (!S_ISREG(st.st_mode)) return s;
    s.regular = true;
    s.size = static_cast<unsigned long long>(st.st_size);
    s.mtime = static_cast<long long>(st.st_mtime);
    s.ctime = static_cast<long long>(st.st_ctime);
#  ifdef __APPLE__
    s.mtimeNs = static_cast<long long>(st.st_mtimespec.tv_nsec);
    s.ctimeNs = static_cast<long long>(st.st_ctimespec.tv_nsec);
#  else
    s.mtimeNs = static_cast<long long>(st.st_mtim.tv_nsec);
    s.ctimeNs = static_cast<long long>(st.st_ctim.tv_nsec);
#  endif
    changeTimeReliable = static_cast<long long>(time(NULL)) - s.ctime > kStampRecentSeconds;
#endif
    const bool hash = hashing == STAMP_HASH || (hashing == STAMP_HASH_IF_NEEDED && !changeTimeReliable);
    if (hash && s.size <= kStampHashLimit) {
        s.hashed = fileContentHash(path, s.hash);   // (left out of comparisons if unreadable)
    }
    return s;
}

// A stamp of `path` now, comparable with `before`: content hashed exactly
// when `before`'s was (whether the change time is still "recent" depends on
// when each stamp is taken).
inline FileStamp fileStampLike(const std::string& path, const FileStamp& before) {
    return fileStamp(path, before.hashed ? STAMP_HASH : STAMP_NO_HASH);
}

// Equivalent of plain `mkdir` (one level: the parent must exist).  Returns
// true if the directory exists afterwards.
inline bool makeDirectory(const std::string& path) {
    if (!isDirectory(path)) {
#ifdef _WIN32
        _mkdir(path.c_str());
#else
        mkdir(path.c_str(), 0755);
#endif
    }
    return isDirectory(path);
}

// Equivalent of `mkdir -p`.  Returns true if the directory exists afterwards.
inline bool makeDirectories(const std::string& path) {
    for (std::string::size_type i = 1; i <= path.size(); ++i) {
#ifdef _WIN32
        const bool partEnds = i == path.size() || path[i] == '/' || path[i] == '\\';
#else
        const bool partEnds = i == path.size() || path[i] == '/';   // (a backslash is ordinary in a POSIX name)
#endif
        if (partEnds) {
            const std::string partial = path.substr(0, i);
            if (!partial.empty()) makeDirectory(partial);
        }
    }
    return isDirectory(path);
}

// `relative` under directory `dir`, with forward slashes and one separator
// between them -- also for a directory that already ends in one
// ("<cwd>/plugins/"): such paths are shown in logs and compared.
inline std::string joinPath(const std::string& dir, const std::string& relative) {
    std::string joined = normalizeSlashes(dir);
    if (!joined.empty() && joined[joined.size() - 1] != '/') joined += '/';
    return joined + relative;
}

// First "<dir>/<relative>" that exists among `dirs`; "" if none.
inline std::string findInDirs(const std::vector<std::string>& dirs, const std::string& relative) {
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (dirs[i].empty()) continue;
        std::string candidate = joinPath(dirs[i], relative);
        if (fileExists(candidate)) return candidate;
    }
    return "";
}

// The same, over a PATH_LIST_SEP-separated plugin search path.
inline std::string findInPluginPath(const std::string& pluginpath, const std::string& relative) {
    return findInDirs(splitPathList(pluginpath), relative);
}

// Immediate subdirectories of `root` (hidden entries skipped), as root + "/" + name.
inline std::vector<std::string> listSubdirectories(const std::string& root) {
    std::vector<std::string> result;
#ifdef _WIN32
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA((root + "/*").c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) return result;
    do {
        if (data.cFileName[0] == '.') continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            result.push_back(root + "/" + data.cFileName);
    } while (FindNextFileA(h, &data));
    FindClose(h);
#else
    DIR* dir = opendir(root.c_str());
    if (!dir) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        std::string child = root + "/" + entry->d_name;
        if (isDirectory(child)) result.push_back(child);
    }
    closedir(dir);
#endif
    std::sort(result.begin(), result.end());
    return result;
}

// Files matching <root>/*/*Plugin.<ext>, sorted (the equivalent of the glob
// pattern PluMA has always used to discover plugins).
inline std::vector<std::string> findPluginFiles(const std::string& root, const std::string& ext) {
    std::vector<std::string> result;
#ifdef _WIN32
    std::vector<std::string> dirs = listSubdirectories(root);
    for (size_t i = 0; i < dirs.size(); ++i) {
        WIN32_FIND_DATAA data;
        HANDLE h = FindFirstFileA((dirs[i] + "/*Plugin." + ext).c_str(), &data);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            // FindFirstFile is case-insensitive and can match 8.3 short
            // names; apply PluMA's own matching rule.
            std::string name = data.cFileName;
            if (isPluginFileName(name, ext))
                result.push_back(dirs[i] + "/" + name);
        } while (FindNextFileA(h, &data));
        FindClose(h);
    }
    std::sort(result.begin(), result.end());
#else
    glob_t globbuf;
    // The root literally: a directory name may hold glob's special characters
    // ("run[1]").
    std::string escapedRoot;
    for (char c : root) {
        if (c == '*' || c == '?' || c == '[' || c == ']' || c == '\\') escapedRoot += '\\';
        escapedRoot += c;
    }
    std::string pattern = escapedRoot + "/*/*Plugin." + ext;
    if (glob(pattern.c_str(), 0, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i)
            result.push_back(globbuf.gl_pathv[i]);
        globfree(&globbuf);   // (defined only after a successful glob())
    }
#endif
    return result;
}

// Load a shared library so that any static initializers (plugin registration)
// run.  Returns NULL on failure; call lastLibraryError() immediately, before
// any other I/O, since console writes can overwrite the Windows error code.
inline void* loadLibrary(const std::string& path) {
#ifdef _WIN32
    std::string native = nativeSlashes(path);
    // LOAD_WITH_ALTERED_SEARCH_PATH (search the plugin's own directory for
    // its dependencies) is only defined for absolute paths.
    fullPathName(native, native);   // unchanged if it fails
    return (void*) LoadLibraryExA(native.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif
}

#ifdef _WIN32
// The absolute directories on PATH, in order.  Relative entries (".", "bin")
// are dropped: they resolve against the current directory, typically a
// pipeline/data directory, which must never supply executables or DLLs.
inline std::vector<std::string> absolutePathDirs() {
    std::vector<std::string> result;
    const char* path = getenv("PATH");
    if (!path) return result;
    std::vector<std::string> entries = splitPathList(path);
    for (size_t i = 0; i < entries.size(); ++i) {
        std::string dir = entries[i];
        if (dir.size() >= 2 && dir[0] == '"' && dir[dir.size() - 1] == '"') dir = dir.substr(1, dir.size() - 2);
        if (isAbsolutePath(dir)) result.push_back(dir);
    }
    return result;
}

// Load `dllName` (e.g. "jvm.dll") before its first use so that a /DELAYLOAD
// import binds to it, trying each of `dirs` in the caller's order.  Only ever
// loads by absolute path: a bare-name LoadLibrary would also search the
// current directory (DLL planting).  Returns false if not found anywhere.
inline bool preloadDll(const std::string& dllName, const std::vector<std::string>& dirs) {
    if (GetModuleHandleA(dllName.c_str())) return true;
    for (size_t i = 0; i < dirs.size(); ++i) {
        // Absolute directories only: a relative one (JAVA_HOME=jdk, a bad
        // registry value) would resolve against the current directory.
        if (!isAbsolutePath(dirs[i])) continue;
        std::string file = normalizeSlashes(dirs[i]) + "/" + dllName;
        if (fileExists(file) && loadLibrary(file)) return true;
    }
    return false;
}

// REG_SZ value from the registry ("" if absent).  `value` NULL = default value.
// Requires advapi32 (linked by SConscript.windows).
inline std::string registryString(HKEY hive, const std::string& key, const char* value) {
    char buf[MAX_PATH * 4];
    DWORD size = sizeof(buf);
    if (RegGetValueA(hive, key.c_str(), value, RRF_RT_REG_SZ, NULL, buf, &size) != ERROR_SUCCESS)
        return "";
    return std::string(buf);
}
#endif

inline std::string lastLibraryError() {
#ifdef _WIN32
    DWORD code = GetLastError();
    char* msg = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, code, 0, (LPSTR) &msg, 0, NULL);
    std::string result = msg ? msg : "unknown error";
    if (msg) LocalFree(msg);
    // System messages end in "\r\n" (sometimes ". \r\n"): one line in the log.
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        result.pop_back();
    return "error " + std::to_string((unsigned long) code) + ": " + result;
#else
    const char* err = dlerror();
    return err ? err : "unknown error";
#endif
}

#ifdef _WIN32
// The running executable's directory (forward slashes), or "" if it cannot
// be determined (never a relative "." that would search the current one).
inline std::string executableDirectory() {
    std::vector<char> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameA(NULL, buf.data(), (DWORD) buf.size());
        if (n == 0) return "";
        if (n < buf.size()) {
            const std::string exe = normalizeSlashes(std::string(buf.data(), n));
            return isAbsolutePath(exe) ? dirName(exe) : "";
        }
        if (buf.size() >= 32768) return "";   // truncated even at the maximum
        buf.resize(buf.size() * 2);
    }
}

// Loads `name` (a PluMA bridge DLL) from the executable's directory; NULL
// with the reason in `why` if it cannot.
// Why a bridge DLL cannot be used: built for another pluma.exe.
inline std::string bridgeMismatch(const std::string& name) {
    return name + " does not match this pluma.exe; rebuild PluMA";
}

inline HMODULE loadSiblingModule(const std::string& name, std::string& why);

// One of PluMA's bridge DLLs next to pluma.exe (pluma_r.dll, pluma_perl.dll),
// checked to be the version this pluma.exe expects: `versionSymbol` exports
// `int (void)` returning `expected`.  NULL, with `why`, otherwise.
inline HMODULE loadBridge(const std::string& name, const char* versionSymbol, int expected, std::string& why) {
    HMODULE bridge = loadSiblingModule(name, why);
    if (!bridge) return NULL;
    typedef int (*BridgeVersion)(void);
    BridgeVersion version = (BridgeVersion) GetProcAddress(bridge, versionSymbol);
    if (!version || version() != expected) {
        why = bridgeMismatch(name);
        return NULL;
    }
    return bridge;
}

inline HMODULE loadSiblingModule(const std::string& name, std::string& why) {
    const std::string dir = executableDirectory();
    if (dir.empty()) {
        why = "the PluMA executable's directory could not be determined";
        return NULL;
    }
    HMODULE module = (HMODULE) loadLibrary(dir + "/" + name);
    if (!module) why = dir + "/" + name + " could not be loaded: " + lastLibraryError();
    return module;
}
#endif

} // namespace pluma

#endif
