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

# Platform flags
is_windows: bool = platform.startswith("win") or platform == "cygwin"
is_darwin: bool = platform.startswith("darwin")
is_linux: bool = platform.startswith("linux")
is_unix: bool = is_darwin or is_linux or platform.startswith("freebsd")


def _read_os_release_id() -> Optional[str]:
    """Read the ID field from /etc/os-release (Linux only)."""
    if not is_linux:
        return None
    try:
        output = subprocess.check_output(
            "grep '^ID=' /etc/os-release",
            shell=True,
            stderr=subprocess.DEVNULL,
        )
        return output.decode().strip().split("=")[1].strip('"')
    except (subprocess.CalledProcessError, IndexError, FileNotFoundError):
        return None


platform_id = _read_os_release_id()
is_alpine: bool = platform_id == "alpine"

# =============================================================================
# Platform-Specific Paths
# =============================================================================

if is_windows:
    # Windows paths
    lib_search_path: List[str] = []
    include_search_path: List[str] = [relpath("./src")]
    
    # Windows uses different separators
    path_separator = "\\"
    shared_lib_ext = ".dll"
    shared_lib_prefix = ""
    executable_ext = ".exe"
else:
    # Unix paths
    lib_search_path: List[str] = [
        "/lib",
        "/usr/lib",
        "/usr/local/lib",
    ]
    include_search_path: List[str] = [
        "/usr/include",
        "/usr/local/include",
        relpath("./src"),
    ]
    
    path_separator = "/"
    shared_lib_prefix = "lib"
    executable_ext = ""
    
    if is_darwin:
        shared_lib_ext = ".dylib"
    else:
        shared_lib_ext = ".so"

# =============================================================================
# Build Directories
# =============================================================================

source_base_dir: str = relpath("./src")
object_base_dir: str = relpath("./obj")
build_base_dir: str = relpath("./out")

if is_windows:
    prefix: str = abspath("C:/Program Files/PluMA")
else:
    prefix: str = abspath("/usr/local")

# =============================================================================
# Compiler Detection
# =============================================================================


def detect_msvc() -> bool:
    """Check if MSVC compiler is available."""
    if not is_windows:
        return False
    try:
        subprocess.check_output(["cl"], stderr=subprocess.STDOUT)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def detect_mingw() -> bool:
    """Check if MinGW compiler is available."""
    if not is_windows:
        return False
    try:
        subprocess.check_output(["gcc", "--version"], stderr=subprocess.STDOUT)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


# Compiler flags
is_msvc: bool = detect_msvc()
is_mingw: bool = detect_mingw() if not is_msvc else False

# =============================================================================
# Module Exports
# =============================================================================

__all__ = [
    "platform",
    "platform_id",
    "is_windows",
    "is_darwin",
    "is_linux",
    "is_unix",
    "is_alpine",
    "is_msvc",
    "is_mingw",
    "lib_search_path",
    "include_search_path",
    "source_base_dir",
    "object_base_dir",
    "build_base_dir",
    "prefix",
    "path_separator",
    "shared_lib_ext",
    "shared_lib_prefix",
    "executable_ext",
]
