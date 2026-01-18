#!python
# Copyright (C) 2017, 2019-2020, 2026 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build support utilities for PluMA.

Provides detection and configuration for:
- Python version detection
- Perl configuration
- Julia installation detection (include paths, library paths)
"""

import os
import subprocess
from dataclasses import dataclass, field
from os import path
from subprocess import PIPE, Popen
from typing import List, Optional
import sys

def _python_version():
    override = os.environ.get("PLUMA_PYTHON_VERSION")
    if override:
        return override
    try:
        output = subprocess.check_output(["python3-config", "--ldversion"], stderr=subprocess.DEVNULL)
        return output.decode().strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ".".join(map(str, sys.version_info[0:2]))


python_version = _python_version()

###################################################################
# HELPER FUNCTIONS
###################################################################


def SourcePath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./src/" + p))
    return res


def ObjectPath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./obj/" + p))
    return res


def LibPath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./lib/" + p))
    return res


def cmdline(command):
    process = Popen(args=command, stdout=PIPE, shell=True)
    return process.communicate()[0].decode("utf8")


def CheckPerl(ctx):
    ctx.Message("Checking Perl configuration... ")

    source = """
    use strict;
    use Config;
    use File::Spec;

    sub search {
        my $paths = shift;
        my $file = shift;
        foreach(@{$paths}) {
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
    """

    f = open(".perltest.pl", "w")
    f.write(source)
    f.close()

    retcode = subprocess.call("perl .perltest.pl", shell=True)

    ctx.Result(retcode == 0)

    os.unlink(".perltest.pl")

    return retcode == 0


###################################################################
# JULIA CONFIGURATION
###################################################################

# Fallback paths for Julia installations on various systems
_JULIA_FALLBACK_PATHS = [
    # Standard system paths
    "/usr/lib/julia",
    "/usr/local/lib/julia",
    # Arch Linux
    "/usr/lib",
    # Ubuntu/Debian with Julia from official binaries
    "/opt/julia/lib",
    # macOS Homebrew
    "/opt/homebrew/lib/julia",
    "/usr/local/opt/julia/lib",
    # Common manual install locations
    "/usr/local/julia/lib",
    os.path.expanduser("~/julia/lib"),
]

_JULIA_INCLUDE_FALLBACK_PATHS = [
    "/usr/include/julia",
    "/usr/local/include/julia",
    "/opt/julia/include/julia",
    "/opt/homebrew/include/julia",
    os.path.expanduser("~/julia/include/julia"),
]


@dataclass
class JuliaConfig:
    """Configuration for Julia installation."""

    julia_home: Optional[str] = None
    include_dir: Optional[str] = None
    lib_dir: Optional[str] = None
    libjulia_path: Optional[str] = None
    include_paths: List[str] = field(default_factory=list)
    lib_paths: List[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        """Check if this configuration has the minimum required paths."""
        return bool(self.include_dir and self.libjulia_path)


def _detect_julia_home_from_env() -> Optional[str]:
    """Detect JULIA_HOME from environment variable."""
    julia_home = os.environ.get("JULIA_HOME")
    if julia_home and os.path.isdir(julia_home):
        return julia_home
    return None


def _detect_julia_home_from_julia() -> Optional[str]:
    """Detect Julia home directory from the julia executable."""
    try:
        # First find julia
        julia_path = subprocess.check_output(
            ["which", "julia"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()

        if not julia_path:
            return None

        # Get BINDIR from Julia itself
        bindir = subprocess.check_output(
            ["julia", "-e", "print(Sys.BINDIR)"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()

        if bindir:
            # BINDIR is typically .../bin, so parent is JULIA_HOME
            julia_home = os.path.dirname(bindir)
            if os.path.isdir(julia_home):
                return julia_home

    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        pass

    return None


def _detect_julia_home_from_fallback() -> Optional[str]:
    """Try to detect Julia home from common installation paths."""
    for lib_path in _JULIA_FALLBACK_PATHS:
        if os.path.isdir(lib_path):
            # Check if libjulia exists here
            for lib_name in ["libjulia.so", "libjulia.dylib"]:
                if os.path.isfile(os.path.join(lib_path, lib_name)):
                    # Return parent as julia_home
                    return os.path.dirname(lib_path)
    return None


def _detect_julia_home() -> Optional[str]:
    """Detect Julia home directory using multiple methods."""
    # Try environment variable first
    julia_home = _detect_julia_home_from_env()
    if julia_home:
        return julia_home

    # Try to get from julia executable
    julia_home = _detect_julia_home_from_julia()
    if julia_home:
        return julia_home

    # Try fallback paths
    return _detect_julia_home_from_fallback()


def _detect_julia_include_dir(julia_home: Optional[str]) -> Optional[str]:
    """Detect Julia include directory."""
    if julia_home:
        # Standard location within JULIA_HOME
        include_dir = os.path.join(julia_home, "include", "julia")
        if os.path.isdir(include_dir) and os.path.isfile(
            os.path.join(include_dir, "julia.h")
        ):
            return include_dir

        # Try just include/
        include_dir = os.path.join(julia_home, "include")
        if os.path.isdir(include_dir) and os.path.isfile(
            os.path.join(include_dir, "julia.h")
        ):
            return include_dir

    # Try fallback paths
    for include_path in _JULIA_INCLUDE_FALLBACK_PATHS:
        if os.path.isdir(include_path) and os.path.isfile(
            os.path.join(include_path, "julia.h")
        ):
            return include_path

    return None


def _find_libjulia(julia_home: Optional[str]) -> Optional[str]:
    """Find the libjulia shared library."""
    lib_names = ["libjulia.so", "libjulia.dylib"]

    if julia_home:
        # Check standard locations within JULIA_HOME
        search_dirs = [
            os.path.join(julia_home, "lib"),
            os.path.join(julia_home, "lib", "julia"),
            julia_home,
        ]

        for search_dir in search_dirs:
            if not os.path.isdir(search_dir):
                continue
            for lib_name in lib_names:
                lib_path = os.path.join(search_dir, lib_name)
                if os.path.isfile(lib_path):
                    return lib_path

    # Try fallback paths
    for lib_dir in _JULIA_FALLBACK_PATHS:
        if not os.path.isdir(lib_dir):
            continue
        for lib_name in lib_names:
            lib_path = os.path.join(lib_dir, lib_name)
            if os.path.isfile(lib_path):
                return lib_path

    return None


def detect_julia_config() -> JuliaConfig:
    """
    Detect Julia installation and return configuration.

    Returns:
        JuliaConfig with detected paths, or empty config if not found.
    """
    config = JuliaConfig()

    # Detect Julia home
    config.julia_home = _detect_julia_home()

    # Detect include directory
    config.include_dir = _detect_julia_include_dir(config.julia_home)

    # Find libjulia
    config.libjulia_path = _find_libjulia(config.julia_home)

    # Build include paths list
    if config.include_dir:
        config.include_paths.append(config.include_dir)
        # Also add parent include dir for headers that use #include <julia.h>
        parent_include = os.path.dirname(config.include_dir)
        if parent_include != config.include_dir and os.path.isdir(parent_include):
            config.include_paths.append(parent_include)

    # Build library paths list
    if config.libjulia_path:
        config.lib_dir = os.path.dirname(config.libjulia_path)
        config.lib_paths.append(config.lib_dir)

    return config


def _log_julia_detection_results(config: JuliaConfig) -> None:
    """Log Julia detection results for debugging."""
    print(f"Julia detection results:")
    print(f"  JULIA_HOME: {config.julia_home or 'not found'}")
    print(f"  Include dir: {config.include_dir or 'not found'}")
    print(f"  Library dir: {config.lib_dir or 'not found'}")
    print(f"  libjulia: {config.libjulia_path or 'not found'}")
    print(f"  Valid: {config.is_valid}")


def CheckJulia(ctx):
    """
    SCons custom test to check for Julia installation.

    Usage in SConstruct:
        conf = Configure(env, custom_tests={'CheckJulia': CheckJulia})
        julia_config = conf.CheckJulia()
        if julia_config and julia_config.is_valid:
            env.AppendUnique(CPPPATH=julia_config.include_paths)
            env.AppendUnique(LIBPATH=julia_config.lib_paths)
            env.Append(LIBS=['julia'])
            env.Append(CPPDEFINES=['HAVE_JULIA'])
    """
    ctx.Message("Checking for Julia installation... ")

    config = detect_julia_config()

    if config.is_valid:
        ctx.Result(f"yes ({config.julia_home or 'system'})")
    else:
        ctx.Result("no")

    return config if config.is_valid else None
