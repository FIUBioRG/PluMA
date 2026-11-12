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

#include "Language.h"
#include "../PluginManager.h"
#include "../platform.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
// The DLLs the PE image at `path` imports (lowercase), read from its import
// table in one pass.  The file is mapped read-only as data -- it is not
// loaded as code -- and every offset is bounds-checked, as a plugin file may
// be truncated or not a PE image at all (then: none).  Delay-load imports
// are listed only `withDelayLoad`: they never make LoadLibrary fail.
static std::set<std::string> peImportedDlls(const std::string& path, bool withDelayLoad = false,
                                            bool* readable = NULL) {
    std::set<std::string> found;
    if (readable) *readable = false;   // until its import directories are located
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return found;
    LARGE_INTEGER fileSize;
    HANDLE mapping = NULL;
    if (GetFileSizeEx(file, &fileSize) && fileSize.QuadPart > 0)
        mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!mapping) return found;
    const unsigned char* base = static_cast<const unsigned char*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    CloseHandle(mapping);
    if (!base) return found;
    const unsigned long long size = static_cast<unsigned long long>(fileSize.QuadPart);
    auto within = [&](unsigned long long offset, unsigned long long length) {
        return offset <= size && length <= size - offset;
    };
    do {
        if (!within(0, sizeof(IMAGE_DOS_HEADER))) break;
        const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) break;
        const unsigned long long ntOffset = static_cast<unsigned long long>(dos->e_lfanew);
        if (!within(ntOffset, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(WORD))) break;
        if (*reinterpret_cast<const DWORD*>(base + ntOffset) != IMAGE_NT_SIGNATURE) break;
        const IMAGE_FILE_HEADER* header = reinterpret_cast<const IMAGE_FILE_HEADER*>(base + ntOffset + sizeof(DWORD));
        const unsigned long long optOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
        // At least the 2-byte magic read next (a truncated/corrupt file may
        // declare an empty optional header).
        if (header->SizeOfOptionalHeader < sizeof(WORD) || !within(optOffset, header->SizeOfOptionalHeader)) break;
        const WORD magic = *reinterpret_cast<const WORD*>(base + optOffset);
        IMAGE_DATA_DIRECTORY imports = { 0, 0 }, delayImports = { 0, 0 };
        // The optional header may hold fewer than the 16 data directories
        // (NumberOfRvaAndSizes): only those present -- declared, and within
        // its declared size -- are read.
        auto directories = [&](size_t directoryOffset, DWORD declared) {
            if (header->SizeOfOptionalHeader < directoryOffset) return false;
            const size_t fit = (header->SizeOfOptionalHeader - directoryOffset) / sizeof(IMAGE_DATA_DIRECTORY);
            const size_t count = std::min<size_t>(declared, fit);
            const IMAGE_DATA_DIRECTORY* dirs =
                reinterpret_cast<const IMAGE_DATA_DIRECTORY*>(base + optOffset + directoryOffset);
            if (count > IMAGE_DIRECTORY_ENTRY_IMPORT) imports = dirs[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (count > IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) delayImports = dirs[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
            return true;
        };
        if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            const size_t at = offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory);
            if (header->SizeOfOptionalHeader < at) break;
            const IMAGE_OPTIONAL_HEADER64* opt = reinterpret_cast<const IMAGE_OPTIONAL_HEADER64*>(base + optOffset);
            if (!directories(at, opt->NumberOfRvaAndSizes)) break;
        } else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            const size_t at = offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory);
            if (header->SizeOfOptionalHeader < at) break;
            const IMAGE_OPTIONAL_HEADER32* opt = reinterpret_cast<const IMAGE_OPTIONAL_HEADER32*>(base + optOffset);
            if (!directories(at, opt->NumberOfRvaAndSizes)) break;
        } else {
            break;
        }
        if (readable) *readable = true;   // (an image may import nothing: a resource-only DLL)
        if (!withDelayLoad) delayImports.VirtualAddress = 0;
        if (imports.VirtualAddress == 0 && delayImports.VirtualAddress == 0) break;
        const unsigned long long sectionsOffset = optOffset + header->SizeOfOptionalHeader;
        const WORD sectionCount = header->NumberOfSections;
        if (!within(sectionsOffset, static_cast<unsigned long long>(sectionCount) * sizeof(IMAGE_SECTION_HEADER))) break;
        const IMAGE_SECTION_HEADER* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(base + sectionsOffset);
        // File offset of an RVA, or ~0 if no section's raw data holds it.
        auto fileOffset = [&](DWORD rva) -> unsigned long long {
            for (WORD i = 0; i < sectionCount; ++i) {
                const DWORD start = sections[i].VirtualAddress;
                const DWORD raw = sections[i].SizeOfRawData;
                if (rva >= start && rva - start < raw)
                    return static_cast<unsigned long long>(sections[i].PointerToRawData) + (rva - start);
            }
            return ~0ULL;
        };
        auto addName = [&](DWORD nameRva) {
            const unsigned long long nameOffset = fileOffset(nameRva);
            if (nameOffset >= size) return;
            const char* name = reinterpret_cast<const char*>(base + nameOffset);
            const size_t maxLength = static_cast<size_t>(std::min<unsigned long long>(size - nameOffset, 260));
            const size_t length = strnlen(name, maxLength);
            if (length == maxLength) return;   // unterminated
            found.insert(pluma::toLowerAscii(std::string(name, length)));
        };
        if (imports.VirtualAddress != 0) {
            unsigned long long descOffset = fileOffset(imports.VirtualAddress);
            for (int n = 0; n < 4096 && within(descOffset, sizeof(IMAGE_IMPORT_DESCRIPTOR));
                 ++n, descOffset += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
                const IMAGE_IMPORT_DESCRIPTOR* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + descOffset);
                if (desc->Name == 0) break;   // the terminating entry
                addName(desc->Name);
            }
        }
        if (delayImports.VirtualAddress != 0) {
            unsigned long long descOffset = fileOffset(delayImports.VirtualAddress);
            for (int n = 0; n < 4096 && within(descOffset, sizeof(IMAGE_DELAYLOAD_DESCRIPTOR));
                 ++n, descOffset += sizeof(IMAGE_DELAYLOAD_DESCRIPTOR)) {
                const IMAGE_DELAYLOAD_DESCRIPTOR* desc =
                    reinterpret_cast<const IMAGE_DELAYLOAD_DESCRIPTOR*>(base + descOffset);
                if (desc->DllNameRVA == 0) break;   // the terminating entry
                addName(desc->DllNameRVA);
            }
        }
    } while (false);
    UnmapViewOfFile(base);
    return found;
}
#elif defined(__linux__)
#include <elf.h>
#elif defined(__APPLE__)
#include "../MachOImports.h"
#endif

// Whether the plugin library at `path` uses the Python C API (so its steps
// on the main thread hold the GIL, see Compiled::executePlugin): itself, or
// through a helper library next to it that it links (one level): undefined
// Py*/_Py* symbols (Linux, macOS), or a loaded library -- on Windows also a
// delay-loaded one -- whose name contains "python" (the runtime, or one
// built on it such as Boost.Python).  When that cannot be read (another
// format or platform, an unreadable file), true: holding the GIL is the safe
// side.  Not seen: Python reached only through dlopen/GetProcAddress (see
// Py::releaseForeignGil), or through a library elsewhere whose name does not
// say so.
// What a library imports: whether it has undefined Py*/_Py* symbols itself
// (ELF, Mach-O), and the libraries it loads (as recorded: a name, or a path
// such as "@rpath/libhelper.dylib").  Per platform below; false if the file
// cannot be read as a library of this platform.
struct LibraryImports {
    bool pythonSymbols = false;
    std::vector<std::string> libraries;
};
#if defined(_WIN32)
static bool readLibraryImports(const std::string& path, LibraryImports& out) {
    bool readable = false;
    const std::set<std::string> imports = peImportedDlls(path, true, &readable);   // (PE: by DLL, not symbol)
    out.libraries.assign(imports.begin(), imports.end());
    return readable;
}
#elif defined(__linux__)
// One ELF class's dynamic symbols and DT_NEEDED entries: whether an
// undefined Py*/_Py* symbol is among them, and the libraries it needs.
// false if the file cannot be read as that class.
template <class Ehdr, class Shdr, class Sym, class Dyn>
static bool scanElf(std::ifstream& file, bool& usesPython, std::vector<std::string>& needed) {
    // Only the headers and the dynamic symbol/string tables are read, not
    // the whole library (CUDA fat binaries can be large).
    Ehdr eh;
    if (!pluma::readFileAt(file, 0, &eh, sizeof(eh)) || eh.e_shentsize != sizeof(Shdr) || eh.e_shnum == 0) return false;
    std::vector<Shdr> sections(eh.e_shnum);
    if (!pluma::readFileAt(file, eh.e_shoff, sections.data(), sections.size() * sizeof(Shdr))) return false;
    const unsigned long long limit = 64ULL << 20;   // a sane bound for one table
    auto table = [&](const Shdr& sh, std::vector<char>& to) {
        if (sh.sh_size > limit) return false;
        to.resize(static_cast<size_t>(sh.sh_size));
        return sh.sh_size == 0 || pluma::readFileAt(file, sh.sh_offset, to.data(), sh.sh_size);
    };
    bool sawDynsym = false;
    // The string tables, by section: .dynsym and .dynamic normally share
    // .dynstr, read once.
    std::map<unsigned long long, std::vector<char> > stringTables;
    for (const Shdr& sh : sections) {
        if ((sh.sh_type != SHT_DYNSYM && sh.sh_type != SHT_DYNAMIC) || sh.sh_link >= sections.size()) continue;
        std::vector<char> data;
        if (!table(sh, data)) return false;
        std::map<unsigned long long, std::vector<char> >::iterator cached = stringTables.find(sh.sh_link);
        if (cached == stringTables.end()) {
            cached = stringTables.insert(std::make_pair(static_cast<unsigned long long>(sh.sh_link),
                                                        std::vector<char>())).first;
            if (!table(sections[sh.sh_link], cached->second)) return false;
        }
        const std::vector<char>& strings = cached->second;
        auto stringAt = [&](unsigned long long offset) -> std::string {
            if (offset >= strings.size()) return std::string();
            const char* begin = strings.data() + offset;
            return std::string(begin, strnlen(begin, strings.size() - static_cast<size_t>(offset)));
        };
        if (sh.sh_type == SHT_DYNSYM) {
            sawDynsym = true;
            const Sym* syms = reinterpret_cast<const Sym*>(data.data());
            for (size_t k = 0; k < data.size() / sizeof(Sym); ++k) {
                if (syms[k].st_shndx != SHN_UNDEF) continue;
                const std::string name = stringAt(syms[k].st_name);
                if (name.compare(0, 2, "Py") == 0 || name.compare(0, 3, "_Py") == 0) usesPython = true;
            }
        } else {
            const Dyn* dyn = reinterpret_cast<const Dyn*>(data.data());
            for (size_t k = 0; k < data.size() / sizeof(Dyn) && dyn[k].d_tag != DT_NULL; ++k)
                if (dyn[k].d_tag == DT_NEEDED) needed.push_back(stringAt(dyn[k].d_un.d_val));
        }
    }
    return sawDynsym;
}
static bool readLibraryImports(const std::string& path, LibraryImports& out) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    unsigned char ident[EI_NIDENT];
    if (!file || !pluma::readFileAt(file, 0, ident, sizeof(ident)) || std::memcmp(ident, ELFMAG, SELFMAG) != 0)
        return false;
    return ident[EI_CLASS] == ELFCLASS64
               ? scanElf<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym, Elf64_Dyn>(file, out.pythonSymbols, out.libraries)
           : ident[EI_CLASS] == ELFCLASS32
               ? scanElf<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym, Elf32_Dyn>(file, out.pythonSymbols, out.libraries)
               : false;
}
#elif defined(__APPLE__)
static bool readLibraryImports(const std::string& path, LibraryImports& out) {
    pluma::macho::Imports imports;
    if (!pluma::macho::readImports(path, imports)) return false;
    for (const std::string& name : imports.undefined)   // C symbols carry a leading '_'
        if (name.compare(0, 3, "_Py") == 0 || name.compare(0, 4, "__Py") == 0) out.pythonSymbols = true;
    out.libraries = imports.libraries;
    return true;
}
#else
static bool readLibraryImports(const std::string&, LibraryImports&) { return false; }
#endif

// Those of `libraries` (file names) that are files next to the library at
// `path`: the helper libraries a plugin ships in its own folder.
static std::vector<std::string> siblingLibraries(const std::string& path, const std::vector<std::string>& libraries) {
    std::vector<std::string> found;
    const std::string dir = pluma::dirName(pluma::normalizeSlashes(path));
    for (const std::string& library : libraries) {
        const std::string sibling = dir + "/" + library;
        if (!library.empty() && pluma::isRegularFile(sibling)) found.push_back(sibling);
    }
    return found;
}

// What the library at `path` and the helper libraries it loads from its own
// folder (one level) import: the libraries' file names ("@rpath/libx.dylib"
// -> "libx.dylib"), and whether any imports Python C API symbols.
// `readable` is false if any of them could not be read.
struct ImportsWithHelpers {
    bool readable = true;
    bool pythonSymbols = false;
    std::vector<std::string> libraries;
};
static ImportsWithHelpers readImportsWithHelpers(const std::string& path) {
    ImportsWithHelpers result;
    auto add = [&result](const LibraryImports& imports) -> std::vector<std::string> {
        std::vector<std::string> bases;
        if (imports.pythonSymbols) result.pythonSymbols = true;
        for (const std::string& lib : imports.libraries) {
            const std::string::size_type slash = lib.find_last_of("/\\");
            bases.push_back(slash == std::string::npos ? lib : lib.substr(slash + 1));
        }
        result.libraries.insert(result.libraries.end(), bases.begin(), bases.end());
        return bases;
    };
    LibraryImports own;
    if (!readLibraryImports(path, own)) {
        result.readable = false;
        return result;
    }
    for (const std::string& helper : siblingLibraries(path, add(own))) {
        LibraryImports imports;
        if (readLibraryImports(helper, imports)) add(imports);
        else result.readable = false;
    }
    return result;
}

static bool libraryUsesPythonApi(const std::string& path) {
    const ImportsWithHelpers imports = readImportsWithHelpers(path);
    if (!imports.readable || imports.pythonSymbols) return true;   // (unreadable: assumed to)
    // The Python runtime itself, or a library built on it: pythonXY.dll,
    // python3.dll, libpython3.12.so, Python.framework's Python,
    // libboost_python312.so, ...
    for (const std::string& lib : imports.libraries)
        if (pluma::toLowerAscii(lib).find("python") != std::string::npos) return true;
    return false;
}

bool Language::libraryUsesPython(const std::string& path) { return libraryUsesPythonApi(path); }

std::string Language::findPluginFile(const std::string& pluginname) const {
    const std::string file = pluma::findInPluginPath(pluginpath, pluginname + "/" + pluginname + "Plugin." + extension);
    if (file.empty()) {
        PluginManager::getInstance().log(language + " plugin " + pluginname + " not found in plugin path.");
        throw PluginNotStarted(language + " plugin " + pluginname + " not found");
    }
    return file;
}

void Language::notEnabled(const std::string& pluginname) {
    PluginManager::getInstance().log(language + " support is not enabled in this build; cannot run " + pluginname + ".");
    throw PluginNotStarted(language + " support is not enabled in this build");
}

void Language::loadPlugin(
    std::string path,
    std::map<std::string, std::string>* pluginLanguages,
    bool list
) {
    // Matches <path>/*/*Plugin.<ext>
    std::vector<std::string> files = pluma::findPluginFiles(path, extension);
    if (!files.empty()) {
        for (size_t i = 0; i < files.size(); i++) {
            std::string filename = files[i];
            // A Rust plugin's library (its folder has a Cargo.toml) is the
            // Rust language's, when that is built in: not a C/C++ plugin.
            if (loadsSharedLibraries() && PluginManager::languageEnabled("Rust") &&
                pluma::fileExists(pluma::dirName(filename) + "/Cargo.toml"))
                continue;
            // File name minus the library prefix and the ".<ext>" suffix,
            // e.g. libFooPlugin.so / FooPlugin.dll / FooPlugin.py -> FooPlugin
            // The plugin's directory names it (<root>/Foo/), so that decides
            // whether a leading "lib" is the library prefix: libFooPlugin.so
            // and FooPlugin.so are both Foo, and libraryScanPlugin.so in
            // libraryScan/ stays libraryScan.  Otherwise strip the prefix
            // only if present (a FooPlugin.so must not become "Plugin").
            std::string base = pluma::baseName(filename);
            const std::string stem = base.substr(0, base.length() - extension.length() - 1);
            const std::string dirPlugin = pluma::baseName(pluma::dirName(filename)) + "Plugin";
            std::string name;
            // ("lib" also for Windows DLLs, whose own prefix is empty: a
            // libFooPlugin.dll built POSIX-style in Foo/ is still Foo.  Only
            // for libraries: a script's file name must match exactly, as its
            // language looks for Foo/FooPlugin.<ext> when it runs.)
            // (On Windows, whose DLLs have no prefix, "lib" as on POSIX, so a
            // library gets the same name on both.)
            const std::string libPrefix = prefix.empty() && loadsSharedLibraries() ? std::string("lib") : prefix;
            if (stem == dirPlugin || stem == prefix + dirPlugin ||
                (loadsSharedLibraries() && stem == "lib" + dirPlugin)) {
                name = dirPlugin;
            } else {
                const bool hasPrefix = !libPrefix.empty() && stem.length() > libPrefix.length() + 6 &&
                                       stem.compare(0, libPrefix.length(), libPrefix) == 0;
                name = stem.substr(hasPrefix ? libPrefix.length() : 0);
            }
            // libFoo/libFooPlugin.so: the directory starts with the library
            // prefix too, so the name is ambiguous -- earlier versions always
            // stripped the prefix (Foo), the directory says libFoo.  A C/C++
            // plugin's real name is what its PluginProxy registers: checked
            // once the library is loaded (below).
            // (On Windows, whose DLLs have no prefix, "lib" as above.)
            std::string alias;
            if (loadsSharedLibraries() && name == stem && stem == dirPlugin &&
                stem.length() > libPrefix.length() + 6 && stem.compare(0, libPrefix.length(), libPrefix) == 0)
                alias = stem.substr(libPrefix.length());
            if (list) {   // `pluma plugins`: name, language and location
                std::cout << "  " << name.substr(0, name.length() - 6) << " (" << language
                          << (enabled() ? "" : ", not enabled in this build") << "): "
                          << filename << std::endl;
            } else {
                // Precedence as in earlier versions: plugin directories are
                // scanned in search-path order (./plugins, then
                // PLUMA_PLUGIN_PATH) and, within one, languages in
                // PluginManager::supported order; a later registration
                // replaces an earlier one.  So the language that runs is the
                // last one found; a C/C++ plugin is the last library loaded
                // (its PluginProxy re-registers the name); a script plugin's
                // file is the first one in the search path (findInPluginPath).
                // One exception: a language compiled out of this build never
                // replaces one that can run.
                std::map<std::string, std::string>::const_iterator existing = pluginLanguages->find(name);
                if (existing != pluginLanguages->end()) {
                    if (!enabled() && PluginManager::languageEnabled(existing->second)) {
                        std::cout << "[PluMA] Note: " << filename << " ignored; " << language
                                  << " is not enabled in this build and " << name << " is provided by a "
                                  << existing->second << " plugin." << std::endl;
                        continue;
                    }
                    if (existing->second == language && !loadsSharedLibraries()) {
                        std::cout << "[PluMA] Note: " << filename << " ignored; the " << language
                                  << " plugin " << name << " earlier in the search path is used." << std::endl;
                        continue;
                    }
                }
                // Reported only once the new registration has really happened.
                auto noteReplacement = [&]() {
                    if (existing == pluginLanguages->end()) return;
                    // Name the file that will actually run: a library is the
                    // one just loaded, but a script plugin runs the first
                    // file of its language in the search path.
                    std::string runs = filename;
                    if (!loadsSharedLibraries()) {
                        const std::string first = pluma::findInPluginPath(
                            pluginpath, pluma::baseName(pluma::dirName(filename)) + "/" + pluma::baseName(filename));
                        if (!first.empty()) runs = first;
                    }
                    std::cout << "[PluMA] Note: the " << language << " plugin " << name << " (" << runs
                              << ") replaces the " << existing->second
                              << " one found earlier (the last one found is used, as before)." << std::endl;
                };
                if (loadsSharedLibraries()) {
                    // The makers registered under the two candidate names before
                    // this library is loaded: which one it registers decides an
                    // ambiguous libFoo/libFooPlugin name (below).
                    auto makerOf = [](const std::string& plugin) -> Maker* {
                        const std::map<std::string, Maker*>& makers = PluginManager::getInstance().makers;
                        std::map<std::string, Maker*>::const_iterator it =
                            makers.find(plugin.substr(0, plugin.length() - 6));   // minus "Plugin"
                        return it == makers.end() ? NULL : it->second;
                    };
                    Maker* const nameMakerBefore = alias.empty() ? NULL : makerOf(name);
                    Maker* const aliasMakerBefore = alias.empty() ? NULL : makerOf(alias);
                    void* handle = pluma::loadLibrary(filename);
#ifdef _WIN32
                    // A plugin linked against the Python C API imports
                    // pythonXY.dll (or python3.dll) directly, which need not be on PATH: load
                    // it by absolute path, as for Python plugins, and retry.
                    // If the retry fails, its own error is reported (e.g. 127
                    // for a plugin built against another Python version); if
                    // Python could not be loaded, the original "not found".
                    // Only for a plugin that actually imports one of them:
                    // any other missing dependency is reported as it is,
                    // without loading Python.
                    // (The load error is saved first: reading the plugin's
                    // import table resets GetLastError().)
                    if (!handle) {
                        const DWORD loadError = GetLastError();
                        bool retried = false;
                        if (loadError == ERROR_MOD_NOT_FOUND) {
                            // pythonXY.dll, or python3.dll for the stable ABI
                            // (its import table names only python3.dll).
                            const std::string runtimeDll = pluma::toLowerAscii(Py::runtimeDllName());
                            // Its own imports and those of helper DLLs next
                            // to it, as for libraryUsesPythonApi.
                            std::set<std::string> imports;
                            for (const std::string& lib : readImportsWithHelpers(filename).libraries)
                                imports.insert(pluma::toLowerAscii(lib));
                            const bool versioned = !runtimeDll.empty() && imports.count(runtimeDll) != 0;
                            const bool stableAbi = !versioned && !runtimeDll.empty() &&
                                                   imports.count("python3.dll") != 0;
                            if ((versioned || stableAbi) && Py::preloadRuntime(stableAbi)) {
                                handle = pluma::loadLibrary(filename);   // its own error if it fails again
                                retried = true;
                            }
                        }
                        if (!retried) SetLastError(loadError);
                    }
#endif
                    if (!handle) {
                        std::string err = pluma::lastLibraryError(); // before any I/O
                        std::cout << "Warning: Null Handle" << std::endl;
                        std::cout << err << std::endl;
                    } else {
                        // The ambiguous libFoo/libFooPlugin case: the name its
                        // PluginProxy just registered (a maker) decides.
                        if (!alias.empty()) {
                            // Registered by this library: a maker that appeared or
                            // changed while it was loaded.
                            const bool registeredName = makerOf(name) != nameMakerBefore;
                            const bool registeredAlias = makerOf(alias) != aliasMakerBefore;
                            if (!registeredName && registeredAlias) {
                                name = alias;
                                // What this registration replaces is now under that name.
                                existing = pluginLanguages->find(name);
                            }
                        }
                        noteReplacement();
                        (*pluginLanguages)[name] = language;
                        // Its library, scanned for Python use when the plugin
                        // first runs (see Compiled::usesPython); a library
                        // replacing an earlier one of the same name decides
                        // for itself.
                        Compiled::setPluginLibrary(name.substr(0, name.length() - 6), filename);
                    }
                } else {
                    noteReplacement();
                    (*pluginLanguages)[name] = language;
                }
            }
        }
    } else {
        std::cout << "Found no " << language << " plugins" << std::endl;
    }
}
