#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# PluMA Build Configuration
# -*- python -*-

"""
SCons build file for PluMA - a plugin-based framework for computational pipelines.

This build system supports:
  - Python, Perl, and R language bindings
  - CUDA acceleration (optional)
  - Rust plugins (experimental)
  - Windows (MSVC and MinGW), macOS, and Linux

Usage:
  scons                    # Build with default options
  scons --without-python   # Disable Python support
  scons --with-cuda        # Enable CUDA support
  scons -c                 # Clean build artifacts
"""

import logging
import os
import subprocess
import sys
from glob import glob
from os import environ, getenv
from os.path import relpath

from resolve_requirements import resolve_and_install

from build_config import (
    include_search_path,
    platform_id,
    is_darwin,
    is_alpine,
    is_windows,
    is_msvc,
    is_mingw,
    shared_lib_ext,
    shared_lib_prefix,
    executable_ext,
)
from build_support import (
    CheckPerl,
    CheckRPackages,
    LibPath,
    ObjectPath,
    SourcePath,
    python_version,
    get_perl_ldopts,
    get_windows_python_config,
    get_msvc_flags,
    get_mingw_flags,
    detect_r_home,
    detect_r_package_path,
    check_r_packages,
)

# =============================================================================
# Constants and Configuration
# =============================================================================

MINIMUM_PYTHON_VERSION = 3
LICENSE = "MIT"

# C++ standard
if is_msvc:
    CXX_STANDARD = "/std:c++14"
    CUDA_CXX_STANDARD = "/std:c++14"
else:
    CXX_STANDARD = "-std=c++11"
    CUDA_CXX_STANDARD = "-std=c++14"

# Required system libraries (platform-specific)
if is_windows:
    REQUIRED_LIBS = []  # Windows doesn't need explicit lib checks
else:
    REQUIRED_LIBS = ["m", "pthread", "dl", "crypt", "pcre", "rt", "c"]

# Clean patterns
CLEAN_PATTERNS_GLOB = [
    ".scon*", "PluGen/*.o", "perm*.txt", "asp_py_*tab.py", "*.out", "*.err",
    "logs/*.log.txt", "pvals.*.txt", "*.pdf", "*.so", "*.dll", "*.pyd",
    "*_wrap.cxx", "*.pyc", "./**/__pycache__",
]

CLEAN_PATTERNS_PATH = [
    "config.log", ".perlconfig.txt", "pluma", "pluma.exe", "PluGen/plugen",
    "PluGen/plugen.exe", "./obj", "./lib", "derep.fasta", "tmp",
    "PerlPluMA.pm", "PyPluMA.py", "RPluMA.R", "__pycache__",
    ".venv", "requirements-plugins.txt",
]

# R library search paths (Unix)
R_LIBRARY_PATHS = [
    "/usr/lib/R/library", "/usr/lib/R/site-library",
    "/usr/local/lib/R/library", "/usr/local/lib/R/site-library",
]

# Language binding configuration
LANGUAGE_BINDINGS = [
    ("without-python", "PyPluMA", f"_PyPluMA{shared_lib_ext}", "-python"),
    ("without-perl", "PerlPluMA", f"PerlPluMA{shared_lib_ext}", "-perl5"),
    ("without-r", "RPluMA", f"RPluMA{shared_lib_ext}", "-r"),
]

# =============================================================================
# Validation
# =============================================================================

if sys.version_info.major < MINIMUM_PYTHON_VERSION:
    logging.error("Python 3 is required to run this SConstruct")
    Exit(1)

# =============================================================================
# Command Line Options
# =============================================================================

OPTIONS = [
    ("--prefix", "prefix", "store", "/usr/local" if not is_windows else "C:/Program Files/PluMA",
     "Installation prefix directory", {"type": "string", "nargs": 1, "metavar": "DIR"}),
    ("--without-python", "without-python", "store_true", False, "Disable Python plugin support", {}),
    ("--without-perl", "without-perl", "store_true", False, "Disable Perl plugin support", {}),
    ("--without-r", "without-r", "store_true", False, "Disable R plugin support", {}),
    ("--with-cuda", "with-cuda", "store_true", False, "Enable CUDA plugin compilation", {}),
    ("--gpu-architecture", "gpu_arch", "store", "sm_35",
     "NVIDIA GPU architecture for CUDA compilation", {"type": "string", "nargs": 1, "metavar": "ARCH"}),
    ("--with-rust", "with-rust", "store_true", False, "Enable experimental Rust plugin support", {}),
    ("--r-include-dir", "r-include-dir", "store", "/usr/local/lib/R/include", "R include directory path", {}),
    ("--r-lib-dir", "r-lib-dir", "store", "/usr/local/lib/R/lib", "R library directory path", {}),
]


def setup_options():
    """Configure all command-line build options."""
    for flag, dest, action, default, help_text, extras in OPTIONS:
        AddOption(flag, dest=dest, action=action, default=default, help=help_text, **extras)


setup_options()

# =============================================================================
# Environment Setup
# =============================================================================


def get_platform_config():
    """Return platform-specific configuration as a dict."""
    if is_windows:
        if is_msvc:
            return get_msvc_flags()
        else:
            return get_mingw_flags()
    elif is_darwin:
        return {"CCFLAGS": ["-DAPPLE"], "CPPDEFINES": ["APPLE"]}
    else:
        return {"LINKFLAGS": ["-rdynamic"], "LIBS": ["rt"]}


def create_base_environment():
    """Create and configure the base SCons environment."""
    if is_windows and is_msvc:
        # Use MSVC environment
        env = Environment(
            ENV=environ,
            CPPDEFINES=["HAVE_PYTHON", "WIN32", "_WINDOWS"],
            CPPPATH=include_search_path,
            LICENSE=[LICENSE],
        )
        msvc_flags = get_msvc_flags()
        for key, value in msvc_flags.items():
            env.Append(**{key: value})
    elif is_windows and is_mingw:
        # Use MinGW environment
        env = Environment(
            ENV=environ,
            tools=["mingw"],
            CC="gcc",
            CXX="g++",
            CPPDEFINES=["HAVE_PYTHON", "WIN32", "_WINDOWS"],
            CCFLAGS=["-Wall", "-O2"],
            CXXFLAGS=["-std=c++14"],
            CPPPATH=include_search_path,
            LICENSE=[LICENSE],
        )
    else:
        # Unix environment
        env = Environment(
            ENV=environ,
            CC=getenv("CC", "cc"),
            CXX=getenv("CXX", "c++"),
            CPPDEFINES=["HAVE_PYTHON"],
            SHCCFLAGS=["-fpermissive", "-fPIC", "-I.", "-O2"],
            SHCXXFLAGS=[CXX_STANDARD, "-fPIC", "-I.", "-O2"],
            CCFLAGS=["-fpermissive", "-fPIC", "-I.", "-O2"],
            CXXFLAGS=[CXX_STANDARD, "-fPIC", "-O2"],
            CPPPATH=include_search_path,
            LICENSE=[LICENSE],
            SHLIBPREFIX="",
        )

    # Apply platform-specific settings
    platform_config = get_platform_config()
    for key, value in platform_config.items():
        env.Append(**{key: value})

    # Alpine Linux / musl libc support
    if is_alpine:
        env.Append(CPPDEFINES=["__MUSL__"])

    # Remove problematic flags on specific platforms
    if not is_windows:
        _remove_annobin_flag(env)

    return env


def _remove_annobin_flag(env):
    """Remove Fedora/RHEL annobin flag if present."""
    annobin_flag = "-specs=/usr/lib/rpm/redhat/redhat-annobin-cc1"
    ccflags = env.get("CCFLAGS", [])
    if annobin_flag in ccflags:
        ccflags.remove(annobin_flag)


def create_cuda_environment():
    """Create environment for CUDA compilation."""
    return Environment(
        ENV=os.environ,
        CUDA_PATH=[getenv("CUDA_PATH", "/usr/local/cuda" if not is_windows else "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.0")],
        CUDA_SDK_PATH=[getenv("CUDA_SDK_PATH", "/usr/local/cuda" if not is_windows else "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.0")],
        NVCCFLAGS=["-I" + os.getcwd(), "--ptxas-options=-v", CUDA_CXX_STANDARD, "-Xcompiler", "-fPIC" if not is_windows else ""],
        GPU_ARCH=GetOption("gpu_arch"),
    )


# =============================================================================
# Language Configuration Functions
# =============================================================================


def configure_python(config):
    """Configure Python language support."""
    if GetOption("without-python"):
        return False

    if is_windows:
        py_config = get_windows_python_config()
        config.env.AppendUnique(
            CPPPATH=[py_config["include_dir"]],
            LIBPATH=[py_config["lib_dir"]],
            LIBS=[py_config["lib_name"]],
        )
    else:
        config.CheckProg("python3-config")
        config.CheckProg("python3")
        config.env.ParseConfig("/usr/bin/python3-config --includes --ldflags")
        config.env.Append(LIBS=["util"])

    return True


def configure_perl(config):
    """Configure Perl language support."""
    if GetOption("without-perl"):
        return False

    config.CheckProg("perl")

    if not is_windows:
        if not config.CheckPerl():
            logging.error("Could not find a valid Perl installation")
            Exit(1)

        config.env.ParseConfig("perl -MExtUtils::Embed -e ccopts -e ldopts")

        if not config.CheckHeader("EXTERN.h"):
            logging.error("Could not find EXTERN.h")
            Exit(1)

        config.env.AppendUnique(
            CXXFLAGS=["-fno-strict-aliasing"],
            CPPDEFINES=["LARGE_SOURCE", "_FILE_OFFSET_BITS=64", "HAVE_PERL", "REENTRANT", "-DWITH_PERL"],
        )

        if is_darwin:
            config.env.Append(LIBS=["crypt", "nsl"])
    else:
        config.env.Append(CPPDEFINES=["HAVE_PERL", "WITH_PERL"])

    return True


def configure_r(config):
    """Configure R language support."""
    if GetOption("without-r"):
        return False

    if not config.CheckProg("R") or not config.CheckProg("Rscript"):
        logging.error("Could not find a valid R installation")
        Exit(1)

    # Check R packages
    if not config.CheckRPackages():
        logging.error("Required R packages (Rcpp, RInside) not found")
        logging.error("Install with: Rscript -e \"install.packages(c('Rcpp', 'RInside'))\"")
        Exit(1)

    # Auto-detect R paths
    r_home = detect_r_home()
    print(f">> R home detected: {r_home or 'not found'}")

    rcpp_include = detect_r_package_path("Rcpp", "include")
    rinside_include = detect_r_package_path("RInside", "include")
    rinside_lib = detect_r_package_path("RInside", "lib")
    
    if not rinside_lib:
        rinside_lib = detect_r_package_path("RInside", "libs")

    print(f">> Rcpp include: {rcpp_include or 'not found'}")
    print(f">> RInside include: {rinside_include or 'not found'}")
    print(f">> RInside lib: {rinside_lib or 'not found'}")

    # Build include/lib paths
    r_include_paths = []
    r_lib_paths = []

    if r_home:
        r_include_paths.append(os.path.join(r_home, "include"))
    if rcpp_include:
        r_include_paths.append(rcpp_include)
    if rinside_include:
        r_include_paths.append(rinside_include)
    if rinside_lib:
        r_lib_paths.append(rinside_lib)

    # Add fallback paths on Unix
    if not is_windows:
        for base_path in R_LIBRARY_PATHS:
            r_include_paths.append(os.path.join(base_path, "Rcpp", "include"))
            r_include_paths.append(os.path.join(base_path, "RInside", "include"))
            r_lib_paths.append(os.path.join(base_path, "RInside", "lib"))
            r_lib_paths.append(os.path.join(base_path, "RInside", "libs"))

    # Apply configuration
    if not is_windows:
        try:
            config.env.ParseConfig("pkg-config --cflags-only-I --libs-only-L libR")
        except:
            pass

        config.env.AppendUnique(
            LDFLAGS=["-Bsymbolic-functions", "-z,relro"],
            CXXFLAGS=[
                "-fno-gnu-unique", "-fpermissive", "--param=ssp-buffer-size=4",
                "-Wformat", "-Wformat-security", "-Werror=format-security", "-Wl,--export-dynamic",
            ],
        )

    config.env.AppendUnique(
        CPPDEFINES=["HAVE_R", "-DWITH_R"],
        CPPPATH=[Dir(p) for p in r_include_paths if os.path.isdir(p)],
        LIBPATH=[Dir(p) for p in r_lib_paths if os.path.isdir(p)],
        LIBS=["R", "RInside"],
    )

    return True


def configure_rust(config):
    """Configure Rust language support."""
    if not GetOption("with-rust"):
        return False

    config.CheckProg("rustc")
    config.CheckProg("cargo")
    return True


def configure_cuda(env_cuda):
    """Configure CUDA environment."""
    if not GetOption("with-cuda"):
        return False

    config_cuda = Configure(env_cuda)
    config_cuda.CheckProg("nvcc")
    config_cuda.CheckHeader("cuda.h")
    config_cuda.Finish()
    return True


# =============================================================================
# Build Target Functions
# =============================================================================


def is_language_enabled(option_name):
    """Check if a language is enabled."""
    return not GetOption(option_name)


def generate_swig_wrappers(env):
    """Generate SWIG wrappers for enabled language bindings."""
    for option, module, _, swig_flag in LANGUAGE_BINDINGS:
        if is_language_enabled(option):
            env.Command(
                module,
                "src/PluginWrapper.i",
                f"swig {swig_flag} -c++ -module $TARGET -o ${{TARGET}}_wrap.cxx $SOURCE",
            )


def build_language_bindings(env):
    """Build shared libraries for enabled language bindings."""
    env.SharedObject(source=SourcePath("PluMA.cxx"), target=ObjectPath("PluMA.os"))

    for option, name, output, _ in LANGUAGE_BINDINGS:
        if is_language_enabled(option):
            wrap_obj = ObjectPath(f"{name}_wrap.os")
            env.SharedObject(source=f"{name}_wrap.cxx", target=wrap_obj)
            env.SharedLibrary(source=[ObjectPath("PluMA.os"), wrap_obj], target=output)


def build_cpp_plugins(env, plugin_path):
    """Compile C++ plugins."""
    print(">> Compiling C++ Plugins")

    for folder in plugin_path:
        for script in Glob(f"{folder}/SConscript"):
            SConscript(script, exports=["env"])

        for plugin in Glob(f"{folder}/*Plugin.cpp"):
            plugin_dir = str(plugin.get_dir())
            sources = Glob(f"{plugin_dir}/*.cpp")
            target = plugin.get_path().replace(".cpp", shared_lib_ext)
            env.SharedLibrary(target=target, source=sources)


def build_cuda_plugins(env_cuda, plugin_path):
    """Compile CUDA plugins."""
    if not GetOption("with-cuda"):
        return

    print(">> Compiling CUDA Plugins")
    env_cuda.AppendUnique(NVCCFLAGS=[f"-I{os.getcwd()}/src", CUDA_CXX_STANDARD])

    for folder in plugin_path:
        for plugin in Glob(f"{folder}/*Plugin.cu"):
            plugin_dir = str(plugin.get_dir())
            plugin_name = os.path.basename(plugin.get_path()).replace(".cu", shared_lib_ext)
            output = f"{plugin_dir}/{shared_lib_prefix}{plugin_name}"
            sources = Glob(f"{plugin_dir}/*.cu", strings=True)
            env_cuda.Command(
                output, sources,
                "nvcc -o $TARGET -std=c++14 -shared $NVCCFLAGS -arch=$GPU_ARCH $SOURCES",
            )


def build_language_objects(env):
    """Build language support objects."""
    languages = Glob("src/languages/*.cxx")

    for language in languages:
        output = language.get_path().replace("src", "obj").replace(".cxx", ".os")
        is_perl = "Perl" in output

        if is_perl and not is_windows:
            perl_ldopts = get_perl_ldopts()
            env.StaticObject(source=language, target=output, LDFLAGS=[[perl_ldopts]])
        else:
            env.StaticObject(source=language, target=output)

    return languages


def build_main_executable(env, languages):
    """Build the main PluMA executable."""
    if is_windows:
        program_libs = [f"python{python_version.replace('.', '')}"]
        if not GetOption("without-perl"):
            program_libs.append("perl")
        if not GetOption("without-r"):
            program_libs.extend(["R", "RInside"])
    else:
        program_libs = [
            "pthread", "m", "dl", "crypt", "c",
            f"python{python_version}", "util", "perl", "R", "RInside",
        ]

    env.Append(LIBPATH=[LibPath("")])
    env.Program(
        target=f"pluma{executable_ext}",
        source=[SourcePath("main.cxx"), SourcePath("PluginManager.cxx"), languages],
        LIBS=program_libs,
    )


def build_plugen(env):
    """Build the PluGen tool."""
    env.Program(f"PluGen/plugen{executable_ext}", Glob("src/PluGen/*.cxx"))


# =============================================================================
# Clean Targets
# =============================================================================


def setup_clean_targets(env):
    """Configure clean targets for build artifacts."""
    glob_items = [Glob(pattern) for pattern in CLEAN_PATTERNS_GLOB]
    path_items = [relpath(pattern) for pattern in CLEAN_PATTERNS_PATH]

    env.Clean("python", [Glob("./**/__pycache__")])
    env.Clean("default", glob_items + path_items)
    env.Clean("all", ["python", "default"])


# =============================================================================
# Configuration Phase
# =============================================================================


def run_configuration(env):
    """Run all configuration checks."""
    custom_tests = {"CheckPerl": CheckPerl, "CheckRPackages": CheckRPackages}
    config = Configure(env, custom_tests=custom_tests)

    # Verify compilers
    if not config.CheckCC():
        Exit(1)
    if not config.CheckCXX():
        Exit(1)
    if not is_windows and not config.CheckSHCXX():
        Exit(1)

    # Check required libraries (Unix only)
    for lib in REQUIRED_LIBS:
        if not config.CheckLib(lib):
            Exit(1)

    config.CheckProg("swig")

    # Configure languages
    configure_python(config)
    configure_perl(config)
    configure_r(config)
    configure_rust(config)

    config.Finish()
    return env


# =============================================================================
# Build Phase
# =============================================================================


def run_build(env, env_cuda):
    """Execute all build steps."""
    # Resolve each plugin's requirements.txt into a shared .venv at the project
    # root so the embedded interpreter picks them up via site.addsitedir() at
    # runtime — keeps users from hand-rolling pip installs per plugin.
    if not GetOption("without-python"):
        resolve_and_install("plugins")

    generate_swig_wrappers(env)
    build_language_bindings(env)

    if not is_windows:
        env.Append(SHLIBPREFIX="lib")

    plugin_path = glob("./plugins/*/")

    build_cpp_plugins(env, plugin_path)

    if env_cuda:
        build_cuda_plugins(env_cuda, plugin_path)

    languages = build_language_objects(env)
    build_plugen(env)
    build_main_executable(env, languages)


# =============================================================================
# Main Entry Point
# =============================================================================


def main():
    """Main build orchestration."""
    print(f">> Platform: {'Windows' if is_windows else 'macOS' if is_darwin else 'Linux'}")
    if is_windows:
        print(f">> Compiler: {'MSVC' if is_msvc else 'MinGW' if is_mingw else 'Unknown'}")

    env = create_base_environment()

    if env.GetOption("clean"):
        setup_clean_targets(env)
        return

    run_configuration(env)

    env_cuda = None
    if GetOption("with-cuda"):
        env_cuda = create_cuda_environment()
        configure_cuda(env_cuda)

    Export("env")
    Export("env_cuda" if env_cuda else {"envPluginCuda": None})

    run_build(env, env_cuda)


main()
