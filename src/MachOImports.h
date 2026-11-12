// Copyright (C) 2026 FIU BioRG
// SPDX-License-Identifier: MIT
//
// What a Mach-O library (macOS) needs from others: its undefined external
// symbols and the libraries it loads -- read from the file's load commands
// and symbol table, without loading it.  No <mach-o/...> headers (the
// layouts used are fixed by the format), so it is also built and tested
// elsewhere.  Universal ("fat") files: every slice is read.

#ifndef PLUMA_MACHO_IMPORTS_H
#define PLUMA_MACHO_IMPORTS_H

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "platform.h"

namespace pluma {
namespace macho {

struct Imports {
    std::vector<std::string> undefined;   // undefined external symbols (with the leading '_')
    std::vector<std::string> libraries;   // install names of the libraries it loads
};

inline uint32_t le32(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline uint32_t be32(const unsigned char* p) {
    return uint32_t(p[3]) | uint32_t(p[2]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[0]) << 24;
}
inline unsigned long long be64(const unsigned char* p) {
    return (static_cast<unsigned long long>(be32(p)) << 32) | be32(p + 4);
}

// One thin 64-bit image at `base` in the file; false if it is not one or
// cannot be read.
inline bool readThin(std::ifstream& file, unsigned long long base, Imports& out) {
    const unsigned long long tableLimit = 64ULL << 20;   // a sane bound for one table
    unsigned char header[32];
    if (!pluma::readFileAt(file, base, header, sizeof(header)) || le32(header) != 0xfeedfacfu) return false;   // MH_MAGIC_64
    const uint32_t ncmds = le32(header + 16), sizeofcmds = le32(header + 20);
    if (sizeofcmds > tableLimit) return false;
    std::vector<unsigned char> cmds(sizeofcmds);
    if (sizeofcmds && !pluma::readFileAt(file, base + sizeof(header), cmds.data(), sizeofcmds)) return false;
    bool sawSymtab = false;
    size_t at = 0;
    for (uint32_t i = 0; i < ncmds; ++i) {
        if (at + 8 > cmds.size()) return false;
        const uint32_t cmd = le32(&cmds[at]), size = le32(&cmds[at + 4]);
        if (size < 8 || size > cmds.size() - at) return false;
        const unsigned char* c = &cmds[at];
        if (cmd == 0x2 && size >= 24) {   // LC_SYMTAB: symoff, nsyms, stroff, strsize
            const uint32_t symoff = le32(c + 8), nsyms = le32(c + 12), stroff = le32(c + 16), strsize = le32(c + 20);
            if (static_cast<unsigned long long>(nsyms) * 16 > tableLimit || strsize > tableLimit) return false;
            std::vector<unsigned char> syms(static_cast<size_t>(nsyms) * 16);
            std::vector<char> strings(strsize);
            if ((!syms.empty() && !pluma::readFileAt(file, base + symoff, syms.data(), syms.size())) ||
                (strsize && !pluma::readFileAt(file, base + stroff, strings.data(), strsize)))
                return false;
            for (uint32_t k = 0; k < nsyms; ++k) {
                const unsigned char* s = &syms[static_cast<size_t>(k) * 16];   // nlist_64
                const uint32_t strx = le32(s);
                const unsigned char type = s[4];
                // External, undefined (N_TYPE == N_UNDF), not a debugging entry (N_STAB).
                if ((type & 0xe0) || (type & 0x0e) != 0 || !(type & 0x01) || strx >= strings.size()) continue;
                const char* name = strings.data() + strx;
                out.undefined.push_back(std::string(name, strnlen(name, strings.size() - strx)));
            }
            sawSymtab = true;
        } else if ((cmd == 0xc || cmd == 0x80000018u || cmd == 0x8000001fu || cmd == 0x20 || cmd == 0x80000023u) &&
                   size >= 24) {
            // LC_LOAD_DYLIB, _WEAK_, LC_REEXPORT_, LC_LAZY_LOAD_, LC_LOAD_UPWARD_: dylib_command.
            const uint32_t nameOffset = le32(c + 8);
            if (nameOffset < size) {
                const char* name = reinterpret_cast<const char*>(c + nameOffset);
                out.libraries.push_back(std::string(name, strnlen(name, size - nameOffset)));
            }
        }
        at += size;
    }
    return sawSymtab;
}

// The imports of the Mach-O file at `path`; false if it is not one (or
// cannot be read).
inline bool readImports(const std::string& path, Imports& out) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    unsigned char magic[8];
    if (!file || !pluma::readFileAt(file, 0, magic, sizeof(magic))) return false;
    const uint32_t fat = be32(magic);
    if (fat == 0xcafebabeu || fat == 0xcafebabfu) {   // FAT_MAGIC, FAT_MAGIC_64
        const bool wide = fat == 0xcafebabfu;
        const uint32_t count = be32(magic + 4);
        // (0xcafebabe is also a Java class file's magic: those have a version, not a small count.)
        if (count == 0 || count > 64) return false;
        const unsigned long long entry = wide ? 32 : 20;
        bool any = false;
        for (uint32_t i = 0; i < count; ++i) {
            unsigned char arch[32];
            if (!pluma::readFileAt(file, 8 + i * entry, arch, entry)) return false;
            const unsigned long long offset = wide ? be64(arch + 8) : be32(arch + 8);
            if (readThin(file, offset, out)) any = true;   // (32-bit slices are skipped)
        }
        return any;
    }
    return readThin(file, 0, out);
}

}  // namespace macho
}  // namespace pluma

#endif
