#! /usr/bin/env python3
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
import subprocess
import sys
from glob import glob
from os import environ, getenv
from os.path import relpath

from resolve_requirements import resolve_and_install
from build_config import include_search_path, platform_id, is_darwin, is_alpine
from build_support import (
    CheckPerl,
    CheckRPackages,
    CheckJava,
    LibPath,
    ObjectPath,
    SourcePath,
    python_version,
    get_perl_ldopts,
    detect_r_config,
    detect_java_config,
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
    ".venv", "requirements-plugins.txt",
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
        CPPDEFINES=[],
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


AddOption(
    "--build-rust-plugins",
    dest="build-rust-plugins",
    action="store_true",
    default=False,
    help="Build all Rust plugins in the plugins directory using cargo"
)

AddOption(
    "--rust-release",
    dest="rust-release",
    action="store_true",
    default=True,
    help="Build Rust plugins in release mode (default: True)"
)

AddOption(
    "--rust-features",
    dest="rust-features",
    type="string",
    nargs=1,
    action="store",
    metavar="FEATURES",
    default="",
    help="Comma-separated list of features to enable for Rust plugins (e.g., 'gpu,cuda')"
)

AddOption(
    "--r-include-dir",
    dest="r-include-dir",
    action="store",
    default="/usr/local/lib/R/include",
    help="Set the include directory for the R installation on the system"
)
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

if not sys.platform.startswith("darwin"):
    env.Append(LINKFLAGS=["-rdynamic"])
    env.Append(LIBS=["rt"])
else:
    env.Append(CCFLAGS=['-DAPPLE'])

if platform_id == "alpine":
    env.Append(CPPDEFINES=["__MUSL__"])

###################################################################

###################################################################
# Either clean folders from previous Scons runs or begin assembling
# `PluMA` and its associated plugins
if env.GetOption("clean"):
    env.Clean("python", [Glob("./**/__pycache__"),])

    env.Clean(
        "default",
        [
            Glob(".scon*"),
            relpath("config.log"),
            relpath(".perlconfig.txt"),
            relpath("pluma"),
            Glob('PluGen/*.o'),
            relpath('PluGen/plugen'),
            relpath("./obj"),
            relpath("./lib"),
            Glob("perm*.txt"),
            Glob("asp_py_*tab.py"),
            Glob("*.out"),
            Glob("*.err"),
            relpath("derep.fasta"),
            Glob("logs/*.log.txt"),
            Glob("pvals.*.txt"),
            #relpath("pythonds"),
            Glob("*.pdf"),
            Glob("*.so"),
            relpath("tmp"),
            Glob("*_wrap.cxx"),
            relpath("PerlPluMA.pm"),
            relpath("PyPluMA.py"),
            relpath("RPluMA.R"),
            relpath("__pycache__"),
            Glob("*.pyc"),
            relpath(".venv"),
            relpath("requirements-plugins.txt"),
            # Rust plugin build artifacts
            Glob("plugins/*/target"),
            Glob("plugins/*/Cargo.lock"),
            Glob("plugins/*/*.so"),
        ],
    )

    # Special target to clean only Rust plugin builds
    env.Clean(
        "rust-plugins",
        [
            Glob("plugins/*/target"),
            Glob("plugins/*/Cargo.lock"),
            Glob("plugins/*/*.so"),
        ],
    )

def configure_python(config):
    """Configure Python language support. Returns True if enabled."""
    if GetOption("without-python"):
        return False

    config.CheckProg("python3-config")
    config.CheckProg("python3")
    config.env.ParseConfig("/usr/bin/python3-config --includes --ldflags")
    config.env.Append(LIBS=["util"])
    config.env.AppendUnique(CPPDEFINES=["HAVE_PYTHON"])
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

    # Base flags that work with both GCC and Clang
    cxx_flags = [
        "-fpermissive",
        "-Wformat", "-Wformat-security", "-Werror=format-security",
    ]

    # GCC-specific flags (check compiler basename, not substring match)
    cxx = config.env.get("CXX", "")
    cxx_basename = os.path.basename(cxx) if cxx else ""
    is_gcc = cxx_basename.startswith("g++") or cxx_basename.startswith("gcc")
    if is_gcc:
        cxx_flags.extend(["-fno-gnu-unique", "--param=ssp-buffer-size=4"])

    config.env.AppendUnique(
        LDFLAGS=["-Bsymbolic-functions", "-z,relro"],
        LINKFLAGS=["-Wl,--export-dynamic"],
        CPPDEFINES=["HAVE_R", "WITH_R"],
        CXXFLAGS=cxx_flags,
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

    java_config = detect_java_config()
    if not java_config or not java_config.is_valid:
        logging.warning("Java toolchain not detected; building without Java support")
        return False

    include_paths = [java_config.include_dir]
    if java_config.platform_include_dir:
        include_paths.append(java_config.platform_include_dir)

    config.env.AppendUnique(
        CPPPATH=include_paths,
        LIBPATH=[java_config.lib_dir] if java_config.lib_dir else [],
        RPATH=[java_config.lib_dir] if java_config.lib_dir else [],
        LIBS=["jvm"],
        CPPDEFINES=["HAVE_JAVA"],
    )
    return True


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

            if getenv("R_INCLUDE_DIR"):
                config.env.AppendUnique(
                    CPPATH=[
                        Dir(getenv("R_INCLUDE_DIR"))
                    ]
                )

            if getenv("R_LIB_DIR"):
                config.env.AppendUnique(
                    LIBPATH=[
                        Dir(getenv("R_LIB_DIR"))
                    ]
                )

            if getenv("RINSIDE_LIB_DIR"):
                config.env.AppendUnique(
                    LIBPATH=[
                        Dir(getenv("RINSIDE_LIB_DIR"))
                    ]
                )

            if getenv("RINSIDE_INCLUDE_DIR"):
                config.env.AppendUnique(
                    CPPPATH=[
                        Dir(getenv("RINSIDE_INCLUDE_DIR"))
                    ]
                )

            if getenv("RCPP_INCLUDE_DIR"):
                config.env.AppendUnique(
                    CPPPATH=[
                        Dir(getenv("RCPP_INCLUDE_DIR"))
                    ]
                )

            config.env.Append(CPPDEFINES=["-DWITH_R"])

    java_enabled = False
    if not env.GetOption("without-java"):
        java_home = getenv("JAVA_HOME")
        if not java_home:
            try:
                javac_path = subprocess.check_output(
                    ["which", "javac"], universal_newlines=True
                ).strip()
                if javac_path:
                    java_home = os.path.dirname(os.path.dirname(os.path.realpath(javac_path)))
            except (subprocess.CalledProcessError, FileNotFoundError):
                java_home = None
        if java_home and os.path.isdir(java_home):
            include_dir = os.path.join(java_home, "include")
            if sys.platform.startswith("linux"):
                platform_dir = "linux"
            elif sys.platform.startswith("darwin"):
                platform_dir = "darwin"
            elif sys.platform.startswith("win"):
                platform_dir = "win32"
            else:
                platform_dir = sys.platform

            cpp_paths = []
            if os.path.isdir(include_dir):
                cpp_paths.append(include_dir)
            platform_include = os.path.join(include_dir, platform_dir)
            if os.path.isdir(platform_include):
                cpp_paths.append(platform_include)

            if cpp_paths:
                config.env.AppendUnique(CPPPATH=[Dir(path) for path in cpp_paths])

            lib_dir = os.path.join(java_home, "lib", "server")
            if not os.path.isdir(lib_dir):
                lib_dir = os.path.join(java_home, "lib")

            if os.path.isdir(lib_dir):
                config.env.AppendUnique(LIBPATH=[Dir(lib_dir)])
                libjvm_path = os.path.join(lib_dir, "libjvm.so")
                libjvm_env = getenv("LIBJVM")
                if (libjvm_env):
                    libjvm_path = libjvm_env
                if os.path.isfile(libjvm_path):
                    config.env.AppendUnique(LIBPATH=[Dir("/usr/lib/jvm/java-1.8.0-openjdk-1.8.0.472.b08-1.el8_10.x86_64/jre/lib/amd64/server/")])
                    config.env.Append(LIBS=["jvm"])
                    config.env.Append(CPPDEFINES=["HAVE_JAVA"])
                    java_enabled = True
                elif config.CheckLib("jvm"):
                    config.env.Append(LIBS=["jvm"])
                    config.env.Append(CPPDEFINES=["HAVE_JAVA"])
                    java_enabled = True
                else:
                    logging.warning(
                        "Java support requested but libjvm could not be linked."
                    )
            else:
                logging.warning("Java support requested but libjvm was not found.")
        else:
            logging.warning("Java support requested but JAVA_HOME/javac could not be resolved.")

    if GetOption("with-rust"):
        config.CheckProg("rustc")
        config.CheckProg("cargo")
        config.env.Append(CPPDEFINES=["HAVE_RUST"])

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


    ###################################################################
    #
    # ###################################################################
    if GetOption("with-cuda"):
        print("!! Compiling CUDA Plugins")
        envPluginCuda.AppendUnique(NVCCFLAGS=["-I"+os.getcwd()+"/src", '-std=c++14'])
        for folder in pluginPath:
            pluginListCU = Glob(folder+'/*Plugin.cu')
            for plugin in pluginListCU:
                pluginName = plugin.get_path()
                pluginName = pluginName.replace(str(plugin.get_dir())+"/", "")
                pluginName = pluginName.replace(".cu", ".so")
                output = str(plugin.get_dir()) + "/lib" + pluginName
                input = Glob(str(plugin.get_dir()) + "/*.cu", strings=True)
                envPluginCuda.Command(
                    output,
                    input,
                    "nvcc -o $TARGET -std=c++14 -shared $NVCCFLAGS -arch=$GPU_ARCH $SOURCES"
                )

    ###################################################################
    # RUST PLUGINS
    # Build Rust plugins when --with-rust or --build-rust-plugins is specified
    # ###################################################################
    def build_rust_plugins(plugin_paths, release_mode=True, features=""):
        """
        Build all Rust plugins found in the given plugin paths.

        Args:
            plugin_paths: List of plugin directory paths to search
            release_mode: If True, build in release mode (optimized)
            features: Comma-separated list of cargo features to enable
        """
        rust_plugins_found = []

        for folder in plugin_paths:
            # Look for Cargo.toml files indicating Rust plugin projects
            cargoFiles = Glob(folder + "/Cargo.toml")
            for cargoFile in cargoFiles:
                pluginDir = str(cargoFile.get_dir())
                # Extract plugin name from directory
                pluginName = pluginDir.split("/")[-1]
                rust_plugins_found.append((pluginDir, pluginName, cargoFile))

        if not rust_plugins_found:
            print("!! No Rust plugins found in plugins directory")
            return

        print("!! Found {} Rust plugin(s) to build".format(len(rust_plugins_found)))

        # Build cargo command options
        cargo_opts = []
        if release_mode:
            cargo_opts.append("--release")
            target_dir = "release"
        else:
            target_dir = "debug"

        if features:
            cargo_opts.append("--features")
            cargo_opts.append(features)

        cargo_opts_str = " ".join(cargo_opts)

        for pluginDir, pluginName, cargoFile in rust_plugins_found:
            print("!!   Building Rust plugin: {} in {}".format(pluginName, pluginDir))

            # Determine the library name from Cargo.toml if possible
            # Default to lib<PluginName>Plugin.so
            output = pluginDir + "/lib" + pluginName + "Plugin.so"

            # Build command that:
            # 1. Changes to plugin directory
            # 2. Runs cargo build with options
            # 3. Copies the built .so file to the plugin directory root
            build_cmd = (
                "cd " + pluginDir + " && "
                "cargo build " + cargo_opts_str + " && "
                "find target/" + target_dir + " -maxdepth 1 -name '*.so' -exec cp {{}} . \\; 2>/dev/null; "
                "if [ -f target/" + target_dir + "/lib" + pluginName + "Plugin.so ]; then "
                "cp target/" + target_dir + "/lib" + pluginName + "Plugin.so .; "
                "elif [ -f target/" + target_dir + "/lib*.so ]; then "
                "cp target/" + target_dir + "/lib*.so lib" + pluginName + "Plugin.so; "
                "fi"
            )

            env.Command(
                output,
                [cargoFile] + Glob(pluginDir + "/src/*.rs"),
                build_cmd
            )

    # Build Rust plugins if either flag is set
    if GetOption("with-rust") or GetOption("build-rust-plugins"):
        print("!! Compiling Rust Plugins")
        build_rust_plugins(
            pluginPath,
            release_mode=GetOption("rust-release"),
            features=GetOption("rust-features") or ""
        )

    ###################################################################
    # Main Executable & PluGen
    env.Append(
        LIBPATH=[LibPath(""),]
    )
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

    env.Program("PluGen/plugen", Glob("src/PluGen/*.cxx"), CPPPATH=[Dir("src")])

def _build_language_object(env, language, output):
    """Build a single language object file."""
    is_perl = "Perl" in output
    ldflags = [[get_perl_ldopts()]] if is_perl else []

    env.StaticObject(source=language, target=output, LDFLAGS=ldflags)


def build_main_executable(env, languages):
    """Build the main PluMA executable."""
    program_libs = [
        "pthread", "m", "dl", "crypt", "c",
        f"python{python_version}", "util", "perl", "R", "RInside",
    ]

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
    """Run all configuration checks and return the configured environment."""
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
    env["JAVA_ENABLED"] = java_enabled
    return env


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


def run_build(env, env_cuda):
    """Execute all build steps."""
    # Merge each plugin's requirements.txt into a shared .venv at the project
    # root so the embedded interpreter picks them up via site.addsitedir() at
    # runtime — keeps users from hand-rolling pip installs per plugin.
    if not GetOption("without-python"):
        resolve_and_install("plugins")

    generate_swig_wrappers(env)
    build_language_bindings(env)

    env.Append(SHLIBPREFIX="lib")
    plugin_path = glob("./plugins/*/")

    build_cpp_plugins(env, plugin_path)

    if env_cuda:
        build_cuda_plugins(env_cuda, plugin_path)

    if env.get("JAVA_ENABLED"):
        build_java_plugins(env, plugin_path)

    languages = build_language_objects(env)
    build_plugen(env)
    build_main_executable(env, languages)


# =============================================================================
# Main Entry Point
# =============================================================================


def main():
    """Main build orchestration."""
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
    if env_cuda:
        Export("env_cuda")

    run_build(env, env_cuda)


main()
