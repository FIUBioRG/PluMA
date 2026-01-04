#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build support utilities for PluMA SConstruct.

This module provides helper functions for:
  - Path resolution (source, object, library directories)
  - Python version detection
  - Perl configuration checking
  - R library detection
  - Windows/MSVC support utilities
"""

import os
import subprocess
import sys
from os import path
from subprocess import PIPE, Popen
from typing import List, Optional, Dict

from build_config import is_windows, is_msvc

# =============================================================================
# Constants
# =============================================================================

_SOURCE_DIR = "./src"
_OBJECT_DIR = "./obj"
_LIB_DIR = "./lib"
_PERL_TEST_SCRIPT = ".perltest.pl"

# =============================================================================
# Python Version Detection
# =============================================================================


def _get_python_version_override() -> str:
    """Get Python version from environment variable."""
    return os.environ.get("PLUMA_PYTHON_VERSION", "")


def _get_python_version_from_config() -> str:
    """Get Python version from python3-config (Unix) or registry (Windows)."""
    if is_windows:
        return ".".join(map(str, sys.version_info[:2]))
    
    try:
        output = subprocess.check_output(
            ["python3-config", "--ldversion"],
            stderr=subprocess.DEVNULL,
        )
        return output.decode().strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def _get_python_version_from_interpreter() -> str:
    """Get Python version from current interpreter."""
    return ".".join(map(str, sys.version_info[:2]))


def _detect_python_version() -> str:
    """Detect the Python version for linking."""
    override = _get_python_version_override()
    if override:
        return override
    
    config_version = _get_python_version_from_config()
    if config_version:
        return config_version
    
    return _get_python_version_from_interpreter()


python_version = _detect_python_version()

# =============================================================================
# Path Helper Functions
# =============================================================================


def _make_absolute_paths(base_dir: str, paths: tuple) -> List[str]:
    """Convert relative paths to absolute paths within a base directory."""
    return [path.abspath(path.join(base_dir, p)) for p in paths]


def SourcePath(*args: str) -> List[str]:
    """Convert relative paths to absolute paths within the source directory."""
    return _make_absolute_paths(_SOURCE_DIR, args)


def ObjectPath(*args: str) -> List[str]:
    """Convert relative paths to absolute paths within the object directory."""
    return _make_absolute_paths(_OBJECT_DIR, args)


def LibPath(*args: str) -> List[str]:
    """Convert relative paths to absolute paths within the library directory."""
    return _make_absolute_paths(_LIB_DIR, args)


# =============================================================================
# Command Execution
# =============================================================================


def cmdline(command: str) -> str:
    """Execute a shell command and return its output."""
    process = Popen(args=command, stdout=PIPE, shell=True)
    output, _ = process.communicate()
    return output.decode("utf8")


def get_perl_ldopts() -> str:
    """Get Perl linker options."""
    if is_windows:
        # On Windows, try to find Perl installation
        try:
            result = subprocess.check_output(
                ["perl", "-MConfig", "-e", "print $Config{libperl}"],
                universal_newlines=True,
                stderr=subprocess.DEVNULL,
            )
            return result.strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            return ""
    else:
        return subprocess.check_output(
            "perl -MExtUtils::Embed -e ldopts",
            universal_newlines=True,
            shell=True,
            encoding="utf8",
        )


# =============================================================================
# Windows/MSVC Support
# =============================================================================


def get_windows_python_config() -> Dict[str, str]:
    """Get Python configuration for Windows builds."""
    config = {
        "include_dir": "",
        "lib_dir": "",
        "lib_name": "",
    }
    
    if not is_windows:
        return config
    
    # Get Python paths
    config["include_dir"] = path.join(sys.prefix, "include")
    config["lib_dir"] = path.join(sys.prefix, "libs")
    
    # Library name varies by version and debug/release
    version = "".join(map(str, sys.version_info[:2]))
    config["lib_name"] = f"python{version}"
    
    return config


def get_msvc_flags() -> Dict[str, List[str]]:
    """Get MSVC-specific compiler and linker flags."""
    if not is_msvc:
        return {"CCFLAGS": [], "CXXFLAGS": [], "LINKFLAGS": [], "CPPDEFINES": []}
    
    return {
        "CCFLAGS": [
            "/W3",           # Warning level 3
            "/EHsc",         # Enable C++ exceptions
            "/MD",           # Use multi-threaded DLL runtime
            "/O2",           # Optimize for speed
            "/DWIN32",
            "/D_WINDOWS",
        ],
        "CXXFLAGS": [
            "/std:c++14",    # C++14 standard
        ],
        "LINKFLAGS": [
            "/SUBSYSTEM:CONSOLE",
        ],
        "CPPDEFINES": [
            "WIN32",
            "_WINDOWS",
            "NOMINMAX",      # Disable min/max macros
            "_CRT_SECURE_NO_WARNINGS",
        ],
    }


def get_mingw_flags() -> Dict[str, List[str]]:
    """Get MinGW-specific compiler and linker flags."""
    return {
        "CCFLAGS": [
            "-Wall",
            "-O2",
        ],
        "CXXFLAGS": [
            "-std=c++14",
        ],
        "LINKFLAGS": [],
        "CPPDEFINES": [
            "WIN32",
            "_WINDOWS",
        ],
    }


# =============================================================================
# R Library Detection (Cross-Platform)
# =============================================================================


def detect_r_home() -> Optional[str]:
    """Detect R home directory."""
    # Check environment variable first
    r_home = os.environ.get("R_HOME")
    if r_home and path.isdir(r_home):
        return r_home
    
    # Try to query R
    try:
        if is_windows:
            result = subprocess.check_output(
                ["Rscript", "-e", "cat(R.home())"],
                stderr=subprocess.DEVNULL,
                universal_newlines=True,
            )
        else:
            result = subprocess.check_output(
                ["Rscript", "-e", "cat(R.home())"],
                stderr=subprocess.DEVNULL,
                universal_newlines=True,
            )
        return result.strip().replace('[1] ', '').strip('"')
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def detect_r_package_path(package: str, subdir: str = "") -> Optional[str]:
    """Detect the installation path of an R package."""
    r_code = f"cat(system.file('{subdir}', package='{package}'))"
    try:
        result = subprocess.check_output(
            ["Rscript", "-e", r_code],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        )
        result = result.strip().replace('[1] ', '').strip('"')
        if result and path.isdir(result):
            return result
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return None


def check_r_packages() -> Dict[str, bool]:
    """Check if required R packages are installed."""
    packages = ["Rcpp", "RInside"]
    results = {}
    
    for package in packages:
        r_code = f"cat(requireNamespace('{package}', quietly=TRUE))"
        try:
            result = subprocess.check_output(
                ["Rscript", "-e", r_code],
                stderr=subprocess.DEVNULL,
                universal_newlines=True,
            )
            results[package] = result.strip() == "TRUE"
        except (subprocess.CalledProcessError, FileNotFoundError):
            results[package] = False
    
    return results


# =============================================================================
# SCons Custom Tests
# =============================================================================

_PERL_CONFIG_SCRIPT = '''
use strict;
use Config;
use File::Spec;

sub search {
    my $paths = shift;
    my $file = shift;
    foreach (@{$paths}) {
        if (-f "$_/$file") {
            return "$_/$file";
            last
        }
    }
    return;
}

my $coredir = File::Spec->catfile($Config{installarchlib}, "CORE");

open(F, ">", ".perlconfig.txt");
print F "perl=$Config{perlpath}\\n";
print F "typemap=" . search(\\@INC, "ExtUtils/typemap") . "\\n";
print F "xsubpp=" . search(\\@INC, "ExtUtils/xsubpp" || search([File::Spec->path()], "xsubpp")) . "\\n";
print F "coredir=$coredir\\n";
close F;
'''


def CheckPerl(ctx) -> bool:
    """SCons custom test to verify Perl configuration."""
    ctx.Message("Checking Perl configuration... ")

    with open(_PERL_TEST_SCRIPT, "w") as f:
        f.write(_PERL_CONFIG_SCRIPT)

    try:
        retcode = subprocess.call(["perl", _PERL_TEST_SCRIPT])
        success = retcode == 0
        ctx.Result(success)
        return success
    finally:
        if path.exists(_PERL_TEST_SCRIPT):
            os.unlink(_PERL_TEST_SCRIPT)


def CheckRPackages(ctx) -> bool:
    """SCons custom test to verify R packages are installed."""
    ctx.Message("Checking R packages (Rcpp, RInside)... ")
    
    results = check_r_packages()
    all_installed = all(results.values())
    
    if not all_installed:
        missing = [pkg for pkg, installed in results.items() if not installed]
        ctx.Result(f"missing: {', '.join(missing)}")
        return False
    
    ctx.Result("found")
    return True


# =============================================================================
# Module Exports
# =============================================================================

__all__ = [
    "python_version",
    "SourcePath",
    "ObjectPath",
    "LibPath",
    "cmdline",
    "get_perl_ldopts",
    "get_windows_python_config",
    "get_msvc_flags",
    "get_mingw_flags",
    "detect_r_home",
    "detect_r_package_path",
    "check_r_packages",
    "CheckPerl",
    "CheckRPackages",
]
