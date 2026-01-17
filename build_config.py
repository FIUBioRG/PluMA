#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build configuration for PluMA.

This module defines platform-specific settings and search paths
used during the build process.
"""

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
is_windows: bool = platform.startswith("win")
is_alpine: bool = platform_id == "alpine"

# =============================================================================
# Search Paths
# =============================================================================

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
    "lib_search_path",
    "include_search_path",
    "source_base_dir",
    "object_base_dir",
    "build_base_dir",
    "prefix",
]
