#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build support utilities for PluMA SConstruct.

This module provides helper functions for:
  - Path resolution (source, object, library directories)
  - Python version detection
  - Perl configuration checking
  - R library detection (Rcpp, RInside)
"""

import logging
import os
import subprocess
import sys
from dataclasses import dataclass, field
from os import path
from subprocess import PIPE, Popen
from typing import List, Optional

# =============================================================================
# Constants
# =============================================================================

_SOURCE_DIR = "./src"
_OBJECT_DIR = "./obj"
_LIB_DIR = "./lib"
_PERL_TEST_SCRIPT = ".perltest.pl"

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

# Fallback R library search paths
_R_FALLBACK_PATHS = [
    "/usr/lib/R/library",
    "/usr/lib/R/site-library",
    "/usr/local/lib/R/library",
    "/usr/local/lib/R/site-library",
]

# =============================================================================
# Python Version Detection
# =============================================================================


def _get_python_version_from_env() -> str:
    """Get Python version from environment variable."""
    return os.environ.get("PLUMA_PYTHON_VERSION", "")


def _get_python_version_from_config() -> str:
    """Get Python version from python3-config."""
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
    """Detect the Python version for linking using priority chain."""
    detectors = [
        _get_python_version_from_env,
        _get_python_version_from_config,
        _get_python_version_from_interpreter,
    ]

    for detector in detectors:
        version = detector()
        if version:
            return version

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
    return subprocess.check_output(
        "perl -MExtUtils::Embed -e ldopts",
        universal_newlines=True,
        shell=True,
        encoding="utf8",
    )


# =============================================================================
# R Library Detection
# =============================================================================


@dataclass
class RConfig:
    """Configuration paths for R and its packages."""

    r_home: Optional[str] = None
    r_include_dir: Optional[str] = None
    rcpp_include_dir: Optional[str] = None
    rinside_include_dir: Optional[str] = None
    rinside_lib_dir: Optional[str] = None

    # Lists of all found paths (for fallback searching)
    include_paths: List[str] = field(default_factory=list)
    lib_paths: List[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        """Check if minimum R configuration was detected."""
        return self.r_home is not None


def _run_r_command(r_code: str) -> Optional[str]:
    """Run an R command and return its output, or None on failure."""
    try:
        result = subprocess.check_output(
            ["Rscript", "-e", r_code],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        )
        return result.strip().replace('[1] ', '').strip('"')
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def _detect_r_home() -> Optional[str]:
    """Detect R home directory."""
    # Try R_HOME environment variable first
    r_home = os.environ.get("R_HOME")
    if r_home and os.path.isdir(r_home):
        return r_home

    # Query R directly
    return _run_r_command("cat(R.home())")


def _detect_r_include_dir(r_home: Optional[str]) -> Optional[str]:
    """Detect R include directory."""
    # Try environment variable
    env_path = os.environ.get("R_INCLUDE_DIR")
    if env_path and os.path.isdir(env_path):
        return env_path

    # Query R
    detected = _run_r_command("cat(R.home('include'))")
    if detected and os.path.isdir(detected):
        return detected

    # Fallback to R_HOME/include
    if r_home:
        fallback = os.path.join(r_home, "include")
        if os.path.isdir(fallback):
            return fallback

    return None


def _detect_r_package_path(package: str, subdir: str = "") -> Optional[str]:
    """Detect the installation path of an R package."""
    r_code = f"cat(system.file('{subdir}', package='{package}'))"
    result = _run_r_command(r_code)

    if result and os.path.isdir(result):
        return result
    return None


def _detect_rcpp_include() -> Optional[str]:
    """Detect Rcpp include directory."""
    # Try environment variable
    env_path = os.environ.get("RCPP_INCLUDE_DIR")
    if env_path and os.path.isdir(env_path):
        return env_path

    # Query R for Rcpp location
    return _detect_r_package_path("Rcpp", "include")


def _detect_rinside_include() -> Optional[str]:
    """Detect RInside include directory."""
    # Try environment variable
    env_path = os.environ.get("RINSIDE_INCLUDE_DIR")
    if env_path and os.path.isdir(env_path):
        return env_path

    # Query R for RInside location
    return _detect_r_package_path("RInside", "include")


def _detect_rinside_lib() -> Optional[str]:
    """Detect RInside library directory."""
    # Try environment variable
    env_path = os.environ.get("RINSIDE_LIB_DIR")
    if env_path and os.path.isdir(env_path):
        return env_path

    # Query R for RInside lib location
    lib_path = _detect_r_package_path("RInside", "lib")
    if lib_path:
        return lib_path

    # Try libs subdirectory (some installations use this)
    libs_path = _detect_r_package_path("RInside", "libs")
    if libs_path:
        return libs_path

    return None


def _collect_fallback_paths() -> tuple:
    """Collect include and lib paths from fallback locations."""
    include_paths = []
    lib_paths = []

    for base_path in _R_FALLBACK_PATHS:
        rcpp_include = os.path.join(base_path, "Rcpp", "include")
        rinside_include = os.path.join(base_path, "RInside", "include")
        rinside_lib = os.path.join(base_path, "RInside", "lib")
        rinside_libs = os.path.join(base_path, "RInside", "libs")

        if os.path.isdir(rcpp_include):
            include_paths.append(rcpp_include)
        if os.path.isdir(rinside_include):
            include_paths.append(rinside_include)
        if os.path.isdir(rinside_lib):
            lib_paths.append(rinside_lib)
        if os.path.isdir(rinside_libs):
            lib_paths.append(rinside_libs)

    return include_paths, lib_paths


def detect_r_config() -> RConfig:
    """
    Automatically detect R and R package configuration.

    Detection priority:
      1. Environment variables (R_HOME, RCPP_INCLUDE_DIR, etc.)
      2. Query R/Rscript directly
      3. Fallback to common installation paths

    Returns:
        RConfig dataclass with detected paths
    """
    config = RConfig()

    # Detect R home and include
    config.r_home = _detect_r_home()
    config.r_include_dir = _detect_r_include_dir(config.r_home)

    # Detect package paths
    config.rcpp_include_dir = _detect_rcpp_include()
    config.rinside_include_dir = _detect_rinside_include()
    config.rinside_lib_dir = _detect_rinside_lib()

    # Collect all found include paths
    for p in [config.r_include_dir, config.rcpp_include_dir, config.rinside_include_dir]:
        if p and p not in config.include_paths:
            config.include_paths.append(p)

    # Collect all found lib paths
    if config.rinside_lib_dir:
        config.lib_paths.append(config.rinside_lib_dir)

    # Add fallback paths for any missing directories
    fallback_includes, fallback_libs = _collect_fallback_paths()

    for p in fallback_includes:
        if p not in config.include_paths:
            config.include_paths.append(p)

    for p in fallback_libs:
        if p not in config.lib_paths:
            config.lib_paths.append(p)

    _log_r_detection_results(config)

    return config


def _log_r_detection_results(config: RConfig) -> None:
    """Log R detection results for debugging."""
    if config.r_home:
        logging.info(f"R home detected: {config.r_home}")
    if config.rcpp_include_dir:
        logging.info(f"Rcpp include detected: {config.rcpp_include_dir}")
    if config.rinside_include_dir:
        logging.info(f"RInside include detected: {config.rinside_include_dir}")
    if config.rinside_lib_dir:
        logging.info(f"RInside lib detected: {config.rinside_lib_dir}")


def check_r_packages() -> dict:
    """
    Check if required R packages are installed.

    Returns:
        Dict mapping package names to their installation status
    """
    packages = ["Rcpp", "RInside"]
    results = {}

    for package in packages:
        r_code = f"cat(requireNamespace('{package}', quietly=TRUE))"
        result = _run_r_command(r_code)
        results[package] = result == "TRUE"

    return results


# =============================================================================
# SCons Custom Tests
# =============================================================================


def _write_perl_test_script() -> None:
    """Write the Perl configuration test script."""
    with open(_PERL_TEST_SCRIPT, "w") as f:
        f.write(_PERL_CONFIG_SCRIPT)


def _run_perl_test() -> int:
    """Run the Perl test script and return exit code."""
    return subprocess.call(["perl", _PERL_TEST_SCRIPT])


def _cleanup_perl_test() -> None:
    """Remove the Perl test script if it exists."""
    if os.path.exists(_PERL_TEST_SCRIPT):
        os.unlink(_PERL_TEST_SCRIPT)


def CheckPerl(ctx) -> bool:
    """
    SCons custom test to verify Perl configuration.

    Args:
        ctx: SCons Configure context

    Returns:
        True if Perl is properly configured, False otherwise
    """
    ctx.Message("Checking Perl configuration... ")

    _write_perl_test_script()

    try:
        retcode = _run_perl_test()
        success = retcode == 0
        ctx.Result(success)
        return success
    finally:
        _cleanup_perl_test()


def CheckRPackages(ctx) -> bool:
    """
    SCons custom test to verify R packages (Rcpp, RInside) are installed.

    Args:
        ctx: SCons Configure context

    Returns:
        True if all required R packages are installed
    """
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
    "CheckPerl",
    "CheckRPackages",
    "RConfig",
    "detect_r_config",
    "check_r_packages",
]
