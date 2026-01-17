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

Usage:
  scons                    # Build with default options
  scons --without-python   # Disable Python support
  scons --with-cuda        # Enable CUDA support
  scons -c                 # Clean build artifacts
"""

import logging
import os
import re
import subprocess
import sys
from glob import glob
from os import environ, getenv
from os.path import relpath

from build_config import include_search_path, platform_id, is_darwin, is_alpine
from build_support import (
    CheckPerl,
    CheckRPackages,
    LibPath,
    ObjectPath,
    SourcePath,
    python_version,
    get_perl_ldopts,
    detect_r_config,
)

# =============================================================================
# Constants and Configuration
# =============================================================================

MINIMUM_PYTHON_VERSION = 3
CXX_STANDARD = "-std=c++11"
CUDA_CXX_STANDARD = "-std=c++14"
LICENSE = "MIT"

REQUIRED_LIBS = ["m", "pthread", "dl", "crypt", "pcre", "rt", "c"]

CLEAN_PATTERNS_GLOB = [
    ".scon*", "PluGen/*.o", "perm*.txt", "asp_py_*tab.py", "*.out", "*.err",
    "logs/*.log.txt", "pvals.*.txt", "*.pdf", "*.so", "*_wrap.cxx", "*.pyc",
    "./**/__pycache__",
]

CLEAN_PATTERNS_PATH = [
    "config.log", ".perlconfig.txt", "pluma", "PluGen/plugen", "./obj", "./lib",
    "derep.fasta", "tmp", "PerlPluMA.pm", "PyPluMA.py", "RPluMA.R", "__pycache__",
]

# Language binding configuration: (option_name, module_name, output_file, swig_flag)
LANGUAGE_BINDINGS = [
    ("without-python", "PyPluMA", "_PyPluMA.so", "-python"),
    ("without-perl", "PerlPluMA", "PerlPluMA.so", "-perl5"),
    ("without-r", "RPluMA", "RPluMA.so", "-r"),
]

# =============================================================================
# Validation
# =============================================================================


def validate_environment():
    """Exit if environment requirements are not met."""
    errors = [
        (sys.version_info.major < MINIMUM_PYTHON_VERSION, "Python 3 is required"),
        (sys.platform.startswith("win"), "Windows is currently not supported"),
    ]
    for condition, message in errors:
        if condition:
            logging.error(message)
            Exit(1)


validate_environment()

# =============================================================================
# Command Line Options
# =============================================================================

# Option definitions: (flag, dest, action, default, help, extras)
OPTIONS = [
    ("--prefix", "prefix", "store", "/usr/local", "Installation prefix directory",
     {"type": "string", "nargs": 1, "metavar": "DIR"}),
    ("--without-python", "without-python", "store_true", False, "Disable Python plugin support", {}),
    ("--without-perl", "without-perl", "store_true", False, "Disable Perl plugin support", {}),
    ("--without-r", "without-r", "store_true", False, "Disable R plugin support", {}),
    ("--without-java", "without-java", "store_true", False, "Disable Java plugin support", {}),
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
    if is_darwin:
        return {"CCFLAGS": ["-DAPPLE"]}
    return {"LINKFLAGS": ["-rdynamic"], "LIBS": ["rt"]}


def create_base_environment():
    """Create and configure the base SCons environment."""
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
    for key, value in get_platform_config().items():
        env.Append(**{key: value})

    # Alpine Linux / musl libc support
    if is_alpine:
        env.Append(CPPDEFINES=["__MUSL__"])

    # Remove problematic Fedora/RHEL annobin flag
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
        CUDA_PATH=[getenv("CUDA_PATH", "/usr/local/cuda")],
        CUDA_SDK_PATH=[getenv("CUDA_SDK_PATH", "/usr/local/cuda")],
        NVCCFLAGS=["-I" + os.getcwd(), "--ptxas-options=-v", CUDA_CXX_STANDARD, "-Xcompiler", "-fPIC"],
        GPU_ARCH=GetOption("gpu_arch"),
    )


# =============================================================================
# Language Configuration Functions
# =============================================================================


def configure_python(config):
    """Configure Python language support. Returns True if enabled."""
    if GetOption("without-python"):
        return False

    config.CheckProg("python3-config")
    config.CheckProg("python3")
    config.env.ParseConfig("/usr/bin/python3-config --includes --ldflags")
    config.env.Append(LIBS=["util"])
    return True


def configure_perl(config):
    """Configure Perl language support. Returns True if enabled."""
    if GetOption("without-perl"):
        return False

    config.CheckProg("perl")
    _verify_perl_installation(config)
    _apply_perl_config(config)
    return True


def _verify_perl_installation(config):
    """Verify Perl is properly installed or exit."""
    if not config.CheckPerl():
        logging.error("Could not find a valid Perl installation")
        Exit(1)

    config.env.ParseConfig("perl -MExtUtils::Embed -e ccopts -e ldopts")

    if not config.CheckHeader("EXTERN.h"):
        logging.error("Could not find EXTERN.h")
        Exit(1)


def _apply_perl_config(config):
    """Apply Perl-specific compiler configuration."""
    config.env.AppendUnique(
        CXXFLAGS=["-fno-strict-aliasing"],
        CPPDEFINES=["LARGE_SOURCE", "_FILE_OFFSET_BITS=64", "HAVE_PERL", "REENTRANT", "WITH_PERL"],
    )
    if is_darwin:
        config.env.Append(LIBS=["crypt", "nsl"])


def configure_r(config):
    """Configure R language support. Returns True if enabled."""
    if GetOption("without-r"):
        return False

    _verify_r_installation(config)
    _verify_r_packages(config)

    r_config = detect_r_config()
    _apply_r_compiler_config(config)
    _apply_r_library_paths(config, r_config)
    return True


def _verify_r_installation(config):
    """Verify R is properly installed or exit."""
    if not config.CheckProg("R") or not config.CheckProg("Rscript"):
        logging.error("Could not find a valid R installation")
        Exit(1)


def _verify_r_packages(config):
    """Verify required R packages are installed."""
    if not config.CheckRPackages():
        logging.error("Required R packages (Rcpp, RInside) not found")
        logging.error("Install with: Rscript -e \"install.packages(c('Rcpp', 'RInside'))\"")
        Exit(1)


def _apply_r_compiler_config(config):
    """Apply R-specific compiler and linker configuration."""
    # Try pkg-config first, fall back gracefully
    try:
        config.env.ParseConfig("pkg-config --cflags-only-I --libs-only-L libR")
    except OSError:
        logging.warning("pkg-config not available for libR, using detected paths")

    config.env.AppendUnique(
        LDFLAGS=["-Bsymbolic-functions", "-z,relro"],
        CPPDEFINES=["HAVE_R", "WITH_R"],
        CXXFLAGS=[
            "-fno-gnu-unique", "-fpermissive", "--param=ssp-buffer-size=4",
            "-Wformat", "-Wformat-security", "-Werror=format-security", "-Wl,--export-dynamic",
        ],
        LIBS=["R", "RInside"],
    )


def _apply_r_library_paths(config, r_config):
    """Add automatically detected R library paths for Rcpp and RInside."""
    print(f">> R configuration detected:")
    print(f"   R home: {r_config.r_home or 'not found'}")
    print(f"   Include paths: {len(r_config.include_paths)} found")
    print(f"   Library paths: {len(r_config.lib_paths)} found")

    config.env.AppendUnique(
        CPPPATH=[Dir(p) for p in r_config.include_paths],
        LIBPATH=[Dir(p) for p in r_config.lib_paths],
    )


def configure_rust(config):
    """Configure Rust language support. Returns True if enabled."""
    if not GetOption("with-rust"):
        return False

    config.CheckProg("rustc")
    config.CheckProg("cargo")
    return True


def configure_java(config):
    """Configure Java language support. Returns True if enabled."""
    if GetOption("without-java"):
        return False

    java_home = _detect_java_home()
    if not java_home:
        logging.warning("Java support requested but JAVA_HOME/javac could not be resolved.")
        return False

    if not _apply_java_paths(config, java_home):
        return False

    return True


def _detect_java_home():
    """Detect Java home directory from environment or javac location."""
    java_home = getenv("JAVA_HOME")
    if java_home and os.path.isdir(java_home):
        return java_home

    try:
        javac_path = subprocess.check_output(
            ["which", "javac"], universal_newlines=True
        ).strip()
        if javac_path:
            return os.path.dirname(os.path.dirname(os.path.realpath(javac_path)))
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


def _get_java_platform_dir():
    """Get the platform-specific subdirectory for Java includes."""
    if sys.platform.startswith("linux"):
        return "linux"
    elif sys.platform.startswith("darwin"):
        return "darwin"
    elif sys.platform.startswith("win"):
        return "win32"
    return sys.platform


def _apply_java_paths(config, java_home):
    """Apply Java include and library paths. Returns True if JVM found."""
    include_dir = os.path.join(java_home, "include")
    platform_dir = _get_java_platform_dir()

    cpp_paths = []
    if os.path.isdir(include_dir):
        cpp_paths.append(include_dir)
    platform_include = os.path.join(include_dir, platform_dir)
    if os.path.isdir(platform_include):
        cpp_paths.append(platform_include)

    if cpp_paths:
        config.env.AppendUnique(CPPPATH=[Dir(p) for p in cpp_paths])

    lib_dir = os.path.join(java_home, "lib", "server")
    if not os.path.isdir(lib_dir):
        lib_dir = os.path.join(java_home, "lib")

    if not os.path.isdir(lib_dir):
        logging.warning("Java support requested but libjvm was not found.")
        return False

    config.env.AppendUnique(LIBPATH=[Dir(lib_dir)])

    libjvm_path = os.path.join(lib_dir, "libjvm.so")
    if os.path.isfile(libjvm_path) or config.CheckLib("jvm"):
        config.env.Append(LIBS=["jvm"])
        config.env.Append(CPPDEFINES=["HAVE_JAVA"])
        return True

    logging.warning("Java support requested but libjvm could not be linked.")
    return False


def configure_cuda(env_cuda):
    """Configure CUDA environment. Returns True if enabled."""
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
    """Check if a language is enabled based on its option."""
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
            _build_single_binding(env, name, output)


def _build_single_binding(env, name, output):
    """Build a single language binding shared library."""
    wrap_obj = ObjectPath(f"{name}_wrap.os")
    env.SharedObject(source=f"{name}_wrap.cxx", target=wrap_obj)
    env.SharedLibrary(source=[ObjectPath("PluMA.os"), wrap_obj], target=output)


def build_cpp_plugins(env, plugin_path):
    """Compile C++ plugins from plugin directories."""
    print(">> Compiling C++ Plugins")

    for folder in plugin_path:
        _process_scons_scripts(env, folder)
        _compile_cpp_plugins_in_folder(env, folder)


def _process_scons_scripts(env, folder):
    """Process any SConscript files in a plugin folder."""
    for script in Glob(f"{folder}/SConscript"):
        SConscript(script, exports=["env"])


def _compile_cpp_plugins_in_folder(env, folder):
    """Compile all C++ plugins in a single folder."""
    for plugin in Glob(f"{folder}/*Plugin.cpp"):
        plugin_dir = str(plugin.get_dir())
        sources = Glob(f"{plugin_dir}/*.cpp")
        target = plugin.get_path().replace(".cpp", ".so")
        env.SharedLibrary(target=target, source=sources)


def build_cuda_plugins(env_cuda, plugin_path):
    """Compile CUDA plugins if CUDA is enabled."""
    if not GetOption("with-cuda"):
        return

    print(">> Compiling CUDA Plugins")
    env_cuda.AppendUnique(NVCCFLAGS=[f"-I{os.getcwd()}/src", CUDA_CXX_STANDARD])

    for folder in plugin_path:
        _compile_cuda_plugins_in_folder(env_cuda, folder)


def _compile_cuda_plugins_in_folder(env_cuda, folder):
    """Compile all CUDA plugins in a single folder."""
    for plugin in Glob(f"{folder}/*Plugin.cu"):
        plugin_dir = str(plugin.get_dir())
        plugin_name = os.path.basename(plugin.get_path()).replace(".cu", ".so")
        output = f"{plugin_dir}/lib{plugin_name}"
        sources = Glob(f"{plugin_dir}/*.cu", strings=True)
        env_cuda.Command(
            output, sources,
            "nvcc -o $TARGET -std=c++14 -shared $NVCCFLAGS -arch=$GPU_ARCH $SOURCES",
        )


def build_java_plugins(env, plugin_path):
    """Compile Java plugins if Java is enabled."""
    print(">> Compiling Java Plugins")

    for folder in plugin_path:
        _compile_java_plugins_in_folder(env, folder)


def _compile_java_plugins_in_folder(env, folder):
    """Compile all Java plugins in a single folder."""
    for plugin in Glob(f"{folder}/*Plugin.java"):
        plugin_dir = str(plugin.get_dir())
        plugin_name = os.path.basename(plugin.get_path()).replace(".java", "")
        output_class = f"{plugin_dir}/{plugin_name}.class"
        sources = Glob(f"{plugin_dir}/*.java", strings=True)

        # Compile all Java files in the plugin directory
        env.Command(
            output_class,
            sources,
            f"javac -d {plugin_dir} $SOURCES",
        )


def build_language_objects(env):
    """Build language support objects and return the language file list."""
    languages = Glob("src/languages/*.cxx")

    for language in languages:
        output = language.get_path().replace("src", "obj").replace(".cxx", ".os")
        _build_language_object(env, language, output)

    return languages


def _build_language_object(env, language, output):
    """Build a single language object file."""
    is_perl = "Perl" in output
    ldflags = [[get_perl_ldopts()]] if is_perl else []

    env.StaticObject(source=language, target=output, LDFLAGS=ldflags)


def build_main_executable(env, languages, java_enabled=False):
    """Build the main PluMA executable."""
    program_libs = [
        "pthread", "m", "dl", "crypt", "c",
        f"python{python_version}", "util", "perl", "R", "RInside",
    ]

    if java_enabled:
        program_libs.append("jvm")

    env.Append(LIBPATH=[LibPath("")])
    env.Program(
        target="pluma",
        source=[SourcePath("main.cxx"), SourcePath("PluginManager.cxx"), languages],
        LIBS=program_libs,
    )


def build_plugen(env):
    """Build the PluGen tool."""
    env.Program("PluGen/plugen", Glob("src/PluGen/*.cxx"))


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
    """Run all configuration checks and return (env, java_enabled)."""
    config = Configure(env, custom_tests={
        "CheckPerl": CheckPerl,
        "CheckRPackages": CheckRPackages,
    })

    _verify_compilers(config)
    _verify_required_libs(config)
    config.CheckProg("swig")

    configure_python(config)
    configure_perl(config)
    configure_r(config)
    configure_rust(config)
    java_enabled = configure_java(config)

    config.Finish()
    return env, java_enabled


def _verify_compilers(config):
    """Verify required compilers are available."""
    checks = [config.CheckCC, config.CheckCXX, config.CheckSHCXX]
    for check in checks:
        if not check():
            Exit(1)


def _verify_required_libs(config):
    """Verify all required libraries are available."""
    for lib in REQUIRED_LIBS:
        if not config.CheckLib(lib):
            Exit(1)


# =============================================================================
# Build Phase
# =============================================================================


def run_build(env, env_cuda, java_enabled=False):
    """Execute all build steps."""
    generate_swig_wrappers(env)
    build_language_bindings(env)

    env.Append(SHLIBPREFIX="lib")
    plugin_path = glob("./plugins/*/")

    build_cpp_plugins(env, plugin_path)

    if env_cuda:
        build_cuda_plugins(env_cuda, plugin_path)

    if java_enabled:
        build_java_plugins(env, plugin_path)

    languages = build_language_objects(env)
    build_plugen(env)
    build_main_executable(env, languages, java_enabled)


# =============================================================================
# Main Entry Point
# =============================================================================


def main():
    """Main build orchestration."""
    env = create_base_environment()

    if env.GetOption("clean"):
        setup_clean_targets(env)
        return

    env, java_enabled = run_configuration(env)

    env_cuda = None
    if GetOption("with-cuda"):
        env_cuda = create_cuda_environment()
        configure_cuda(env_cuda)

    Export("env")
    if env_cuda:
        Export("env_cuda")

    run_build(env, env_cuda, java_enabled)


main()
