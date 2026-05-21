#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build configuration for PluMA.

This module defines platform-specific settings and search paths
used during the build process.
"""

import os
import subprocess
import sys
from os.path import abspath, relpath
from typing import List, Optional

# =============================================================================
# Platform Detection
# =============================================================================

platform = sys.platform


def _read_os_release_id() -> Optional[str]:
    """Read the ID field from /etc/os-release."""
    try:
        output = subprocess.check_output(
            "grep '^ID=' /etc/os-release",
            shell=True,
            stderr=subprocess.DEVNULL,
        )
        return output.decode().strip().split("=")[1].strip('"')
    except (subprocess.CalledProcessError, IndexError):
        return None


def _detect_platform_id() -> Optional[str]:
    """Detect the Linux distribution ID from /etc/os-release."""
    if not platform.startswith("linux"):
        return None
    return _read_os_release_id()


platform_id = _detect_platform_id()

# =============================================================================
# Platform Flags (Pre-computed for reduced complexity in consumers)
# =============================================================================

is_darwin: bool = platform.startswith("darwin")
is_linux: bool = platform.startswith("linux")
is_windows: bool = platform.startswith("win") or platform == "cygwin"
is_alpine: bool = platform_id == "alpine"
is_unix: bool = is_darwin or is_linux or platform.startswith(("freebsd", "openbsd", "netbsd"))

# Windows toolchain detection. MSVC is selected when the user has loaded a
# Visual Studio developer environment (VSCMD_ARG_TGT_ARCH is exported by
# vcvarsall.bat) or has explicitly pointed CC/CXX at cl.exe. MinGW is the
# fallback Windows toolchain.
_cc = os.environ.get("CC", "").lower()
is_msvc: bool = is_windows and (
    "vscmd_arg_tgt_arch" in {k.lower() for k in os.environ}
    or _cc.endswith("cl") or _cc.endswith("cl.exe")
)
is_mingw: bool = is_windows and not is_msvc

# =============================================================================
# Platform-Specific Filename Conventions
# =============================================================================

if is_windows:
    shared_lib_ext: str = ".dll"
    shared_lib_prefix: str = ""
    executable_ext: str = ".exe"
    path_separator: str = "\\"
elif is_darwin:
    shared_lib_ext = ".dylib"
    shared_lib_prefix = "lib"
    executable_ext = ""
    path_separator = "/"
else:
    shared_lib_ext = ".so"
    shared_lib_prefix = "lib"
    executable_ext = ""
    path_separator = "/"

# =============================================================================
# Search Paths
# =============================================================================

if is_windows:
    # Windows doesn't have the Unix /lib / /usr/lib convention; downstream
    # detectors (Python, Perl, R, Java) discover headers and import libraries
    # through their respective tooling instead.
    lib_search_path: List[str] = []
    include_search_path: List[str] = [relpath("./src")]
else:
    lib_search_path = [
        "/lib",
        "/usr/lib",
        "/usr/local/lib",
    ]
    include_search_path = [
        "/usr/include",
        "/usr/local/include",
        relpath("./src"),
    ]

# =============================================================================
# Build Directories
# =============================================================================

source_base_dir: str = relpath("./src")
object_base_dir: str = relpath("./obj")
build_base_dir: str = relpath("./out")
prefix: str = abspath("/usr/local")

# =============================================================================
# Module Exports
# =============================================================================

__all__ = [
    "platform",
    "platform_id",
    "is_darwin",
    "is_linux",
    "is_windows",
    "is_alpine",
    "is_unix",
    "is_msvc",
    "is_mingw",
    "shared_lib_ext",
    "shared_lib_prefix",
    "executable_ext",
    "path_separator",
    "lib_search_path",
    "include_search_path",
    "source_base_dir",
    "object_base_dir",
    "build_base_dir",
    "prefix",
]
