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

Windows (MSVC) builds are described in SConscript.windows, which this file
hands over to after defining the command-line options.
"""

import logging
import os
import shlex
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
    shared_lib_ext,
    shared_lib_prefix,
    executable_ext,
)
from build_support import (
    CheckPerl,
    CheckRPackages,
    CheckJava,
    CheckJulia,
    LibPath,
    ObjectPath,
    SourcePath,
    python_version,
    python_version_from_includes,
    compiler_is_clang,
    detect_r_config,
    detect_java_config,
    detect_julia_config,
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
    "logs/*.log.txt", "pvals.*.txt", "*.pdf", "*.so", "*.pyc",
    "./**/__pycache__",
]

CLEAN_PATTERNS_PATH = [
    "config.log", ".perlconfig.txt", "pluma", "PluGen/plugen", "./obj", "./lib",
    "derep.fasta", "tmp", "PerlPluMA.pm", "PyPluMA.py", "__pycache__",
    ".venv", "requirements-plugins.txt",
    # Not "*_wrap.cxx": RPluMA_wrap.cxx is tracked in git (the fallback for
    # builds without swig).  Not RPluMA.R either: only swig can write it, and
    # without swig a later build reuses the one from an earlier build.
    "PyPluMA_wrap.cxx", "PerlPluMA_wrap.cxx",
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
    ("--with-julia", "with-julia", "store_true", False, "Enable experimental Julia plugin support (requires libjulia; off by default)", {}),
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
        # Plugins (dylibs) use PluginManager symbols defined in the pluma
        # executable; macOS resolves them at load time only when told to.
        return {"CCFLAGS": ["-DAPPLE"], "SHLINKFLAGS": ["-undefined", "dynamic_lookup"]}
    return {"LINKFLAGS": ["-rdynamic"], "LIBS": ["rt"]}


def _gcc_compile_flags(shared):
    """gcc/clang-style flags, with -fPIC only meaningful for shared objects."""
    return ["-fpermissive", "-I.", "-O2", "-fPIC"]


def create_base_environment():
    """Create and configure the base SCons environment."""
    env_kwargs = dict(
        CC=getenv("CC", "cc"),
        CXX=getenv("CXX", "c++"),
        CPPDEFINES=[],
        SHCCFLAGS=_gcc_compile_flags(shared=True),
        SHCXXFLAGS=[CXX_STANDARD] + _gcc_compile_flags(shared=True),
        CCFLAGS=_gcc_compile_flags(shared=False),
        CXXFLAGS=[CXX_STANDARD] + _gcc_compile_flags(shared=False),
        CPPPATH=include_search_path,
        LICENSE=[LICENSE],
        SHLIBPREFIX=shared_lib_prefix,
        SHLIBSUFFIX=shared_lib_ext,
        PROGSUFFIX=executable_ext,
    )
    env = Environment(ENV=environ, **env_kwargs)

    # Apply platform-specific settings
    for key, value in get_platform_config().items():
        env.Append(**{key: value})

    # Alpine Linux / musl libc support
    if is_alpine:
        env.Append(CPPDEFINES=["__MUSL__"])

    # Remove problematic Fedora/RHEL annobin flag
    _remove_annobin_flag(env)

    return env


def _strip_lto_flags(env):
    """Strip -flto flags injected by perl ExtUtils::Embed ldopts.

    Why: perl's ldopts on RHEL/Fedora ship `-flto=auto`, which makes clang emit
    LTO bytecode that the default bfd linker cannot read, breaking the link of
    objects that don't use perl (e.g. PluGen).
    """
    for key in ("CCFLAGS", "CXXFLAGS", "SHCCFLAGS", "SHCXXFLAGS", "LINKFLAGS", "SHLINKFLAGS"):
        flags = env.get(key)
        if not flags:
            continue
        env[key] = [f for f in flags if not (isinstance(f, str) and f.startswith("-flto"))]


def _remove_annobin_flag(env):
    """Strip Fedora/RHEL hardening specs injected via perl ccopts / python config.

    Why: `-specs=...redhat-annobin-cc1` makes gcc load gcc-annobin.so, a plugin
    that is version-locked to the compiler it was built for. Under a gcc-toolset
    / devtoolset mismatch (e.g. plugin built for gcc 8.5.0 but compiling with gcc
    9.5.0) cc1plus aborts with "fail to initialize plugin gcc-annobin.so". The
    spec can land in any compile-flag list depending on which config injected it,
    so filter them all rather than CCFLAGS alone.
    """
    specs = ("redhat-annobin-cc1", "redhat-hardened-cc1", "redhat-hardened-ld")
    for key in ("CCFLAGS", "CXXFLAGS", "SHCCFLAGS", "SHCXXFLAGS", "CPPFLAGS", "LINKFLAGS", "SHLINKFLAGS"):
        flags = env.get(key)
        if not flags:
            continue
        env[key] = [f for f in flags if not any(s in str(f) for s in specs)]


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

# =============================================================================
# Windows (MSVC): described in SConscript.windows, which uses the options
# defined above; everything below is the POSIX (GCC/Clang) build.
# =============================================================================

if is_windows:
    SConscript("SConscript.windows")
    Return()


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

    global python_version
    config.CheckProg("python3-config")
    config.CheckProg("python3")
    # The Python 3.14 installation under /usr/share where it exists (as
    # before); otherwise the python3-config on PATH.
    python_config = "/usr/share/python3.14/bin/python3-config"
    if not os.path.isfile(python_config):
        python_config = config.env.WhereIs("python3-config")
    if not python_config:
        logging.error("python3-config not found; install the Python development headers "
                      "or build with --without-python.")
        Exit(1)
    # --includes run once: its flags for the build, and its include directory
    # names the libpython of that same installation to link (the default
    # python_version may be another installation's); PLUMA_PYTHON_VERSION
    # still overrides.
    python_includes = subprocess.check_output([python_config, "--includes"]).decode()
    config.env.MergeFlags(python_includes)
    # (Quoted: the python3-config found may live under a path with spaces.)
    config.env.ParseConfig(shlex.quote(python_config) + " --ldflags")
    python_version = python_version_from_includes(python_includes)
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

    _strip_lto_flags(config.env)

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

    # GCC-specific flags (clang -- the Docker image's compiler, macOS's c++ --
    # rejects them).  Asked from the compiler: a plain "c++" may be either.
    if not compiler_is_clang(config.env.subst("$CXX")):
        cxx_flags.extend(["-fno-gnu-unique", "--param=ssp-buffer-size=4"])

    config.env.AppendUnique(
        LDFLAGS=["-Bsymbolic-functions", "-z,relro"],
        LINKFLAGS=["-Wl,--export-dynamic"],
        # ENABLE_LEGACY_NONAPI re-exposes SET_S4_OBJECT and other legacy R API
        # symbols that R 4.5+ moved behind a feature flag; SWIG's R bindings
        # still emit calls to them, so the wrapper won't compile without it.
        CPPDEFINES=["HAVE_R", "WITH_R", "ENABLE_LEGACY_NONAPI"],
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

    # libR and libRInside live in R's own trees, which are not on the dynamic
    # loader's path: record in pluma's RPATH every library directory pluma
    # links them from, so it runs as built.
    def _r_runtime_dirs(libpath):
        dirs = []
        for entry in libpath:
            d = entry.abspath if hasattr(entry, "abspath") else str(entry)
            try:
                names = os.listdir(d) if os.path.isdir(d) else []
            except OSError:   # e.g. not readable by the build user: not searched
                names = []
            if any(f.startswith(("libR.", "libRInside.")) for f in names):
                dirs.append(d)
        return dirs
    config.env.AppendUnique(RPATH=_r_runtime_dirs(config.env.get("LIBPATH", [])))


def configure_rust(config):
    """Configure Rust language support. Returns True if enabled."""
    if not GetOption("with-rust"):
        return False

    config.CheckProg("rustc")
    config.CheckProg("cargo")
    config.env.AppendUnique(CPPDEFINES=["HAVE_RUST"])
    return True


def configure_julia(config):
    """Configure Julia language support. Off by default, opt-in via --with-julia.

    Returns True only when the feature flag is set AND a usable libjulia
    is found on the system. A missing-libjulia case downgrades to a
    warning rather than an error so the rest of the build still succeeds.
    """
    if not GetOption("with-julia"):
        return False

    julia_config = detect_julia_config()
    if not julia_config or not julia_config.is_valid:
        logging.warning(
            "Julia support requested via --with-julia but libjulia was not "
            "found. Set $JULIA_HOME or ensure 'julia' is on PATH and points "
            "at a development install. Building without Julia support."
        )
        return False

    config.env.AppendUnique(CPPPATH=[Dir(p) for p in julia_config.include_paths])
    config.env.AppendUnique(LIBPATH=[Dir(p) for p in julia_config.lib_paths])
    config.env.Append(LIBS=["julia"])
    config.env.AppendUnique(CPPDEFINES=["HAVE_JULIA"])
    logging.info(f"Julia support enabled (JULIA_HOME: {julia_config.julia_home})")
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
    """Generate SWIG wrappers for enabled language bindings.  Returns the
    modules to build: {module name: wrapper source}.

    The targets are the files swig actually writes, so the wrappers depend on
    src/PluginWrapper.i and are regenerated whenever it changes.  PyPluMA and
    PerlPluMA have no committed wrapper: without swig they are skipped (with
    a notice), so the rest of the build -- and the R module, from the
    committed RPluMA_wrap.cxx -- still succeeds.  With swig, the R wrapper is
    generated into obj/ (the tracked file does not churn with each machine's
    swig version); RPluMA.R goes next to RPluMA.so as before."""
    have_swig = env.WhereIs("swig")
    modules = {}
    if is_language_enabled("without-python"):
        if have_swig:
            env.Command(["PyPluMA_wrap.cxx", "PyPluMA.py"], "src/PluginWrapper.i",
                        "swig -python -c++ -threads -module PyPluMA -o ${TARGETS[0]} $SOURCE")
            modules["PyPluMA"] = "PyPluMA_wrap.cxx"
        else:
            print("[PluMA] swig not found; skipping the PyPluMA helper module "
                  "(Python plugins that import PyPluMA will not run).")
    if is_language_enabled("without-perl"):
        if have_swig:
            env.Command(["PerlPluMA_wrap.cxx", "PerlPluMA.pm"], "src/PluginWrapper.i",
                        "swig -perl5 -c++ -module PerlPluMA -o ${TARGETS[0]} $SOURCE")
            modules["PerlPluMA"] = "PerlPluMA_wrap.cxx"
        else:
            print("[PluMA] swig not found; skipping the PerlPluMA helper module "
                  "(Perl plugins that use PerlPluMA will not run).")
    if is_language_enabled("without-r"):
        r_wrapper = "RPluMA_wrap.cxx"
        if have_swig:
            r_wrapper = ObjectPath("RPluMA_wrap.cxx")[0]
            env.Command([r_wrapper, "RPluMA.R"], "src/PluginWrapper.i",
                        "swig -r -c++ -module RPluMA -outdir ${TARGETS[1].dir} -o ${TARGETS[0]} $SOURCE")
        elif os.path.isfile("RPluMA.R"):
            print("[PluMA] swig not found; using the committed RPluMA_wrap.cxx and the existing RPluMA.R.")
        else:
            print("[PluMA] WARNING: swig not found and RPluMA.R is missing: R plugins that load "
                  "RPluMA will not run. Install swig and rebuild.")
        modules["RPluMA"] = r_wrapper
    return modules


def build_language_bindings(env, modules):
    """Build shared libraries for the language bindings generate_swig_wrappers
    set up (only enabled languages: R's needs R's headers)."""
    env.SharedObject(source=SourcePath("PluMA.cxx"), target=ObjectPath("PluMA.os"))

    for option, name, output, _ in LANGUAGE_BINDINGS:
        if name in modules:
            wrap_obj = ObjectPath(f"{name}_wrap.os")
            env.SharedObject(source=modules[name], target=wrap_obj)
            # The module's exact file name (_PyPluMA.so, PerlPluMA.so,
            # RPluMA.so), as the languages load it: no "lib" prefix.
            env.SharedLibrary(source=[ObjectPath("PluMA.os"), wrap_obj], target=output, SHLIBPREFIX="")


def build_cpp_plugins(env, plugin_path):
    """Compile C++ plugins from plugin directories."""
    print(">> Compiling C++ Plugins")

    for folder in plugin_path:
        _process_scons_scripts(env, folder)
        _compile_cpp_plugins_in_folder(env, folder)


# The CUDA environment plugin SConscripts get a copy of (set in run_build).
plugin_env_cuda = None


def _process_scons_scripts(env, folder):
    """Process any SConscript files in a plugin folder.  The plugin builds
    itself with `env` and `envPluginCuda` -- copies, so what one plugin adds
    (defines, link flags) reaches neither pluma itself nor the other plugins
    (as on Windows, SConscript.windows)."""
    for script in Glob(f"{folder}/SConscript"):
        SConscript(script, exports={"env": env.Clone(),
                                    "envPluginCuda": plugin_env_cuda.Clone() if plugin_env_cuda else None})


def _compile_cpp_plugins_in_folder(env, folder):
    """Compile all C++ plugins in a single folder -- unless it has its own
    SConscript, which builds it alone (building its *Plugin.cpp here too would
    define the same target twice)."""
    if Glob(f"{folder}/SConscript"):
        return
    for plugin in Glob(f"{folder}/*Plugin.cpp"):
        plugin_dir = str(plugin.get_dir())
        sources = Glob(f"{plugin_dir}/*.cpp")
        # The extension PluMA looks for: "dylib" on macOS, "so" elsewhere.
        target = plugin.get_path().replace(".cpp", shared_lib_ext)
        env.SharedLibrary(target=target, source=sources)


def build_cuda_plugins(env_cuda, plugin_path):
    """Compile CUDA plugins if CUDA is enabled."""
    if not GetOption("with-cuda"):
        return

    print(">> Compiling CUDA Plugins")
    for folder in plugin_path:
        _compile_cuda_plugins_in_folder(env_cuda, folder)


def build_rust_plugins(env, plugin_path, release_mode=True, features=""):
    """Build every Rust plugin (a folder with a Cargo.toml) with cargo, copying
    the library to <folder>/lib<Name>Plugin.so."""
    rust_plugins = []
    for folder in plugin_path:
        for cargo_file in Glob(f"{folder}/Cargo.toml"):
            plugin_dir = str(cargo_file.get_dir())
            rust_plugins.append((plugin_dir, os.path.basename(plugin_dir), cargo_file))

    if not rust_plugins:
        print(">> No Rust plugins found in plugins directory")
        return

    print(f">> Found {len(rust_plugins)} Rust plugin(s) to build")
    cargo_opts = ["--release"] if release_mode else []
    target_dir = "release" if release_mode else "debug"
    if features:
        cargo_opts += ["--features", features]
    cargo_opts_str = " ".join(cargo_opts)

    for plugin_dir, plugin_name, cargo_file in rust_plugins:
        print(f">>   Building Rust plugin: {plugin_name} in {plugin_dir}")
        output = f"{plugin_dir}/lib{plugin_name}Plugin.so"
        build_cmd = (
            f"cd {plugin_dir} && cargo build {cargo_opts_str} && "
            f"if [ -f target/{target_dir}/lib{plugin_name}Plugin.so ]; then "
            f"cp target/{target_dir}/lib{plugin_name}Plugin.so .; "
            f"else cp target/{target_dir}/lib*.so lib{plugin_name}Plugin.so; fi"
        )
        env.Command(output, [cargo_file] + Glob(f"{plugin_dir}/src/*.rs"), build_cmd)


def _compile_cuda_plugins_in_folder(env_cuda, folder):
    """Compile all CUDA plugins in a single folder."""
    for plugin in Glob(f"{folder}/*Plugin.cu"):
        plugin_dir = str(plugin.get_dir())
        plugin_name = os.path.basename(plugin.get_path()).replace(".cu", shared_lib_ext)
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
    """The language support sources, compiled by the pluma Program.  (Perl's
    embedding flags come from the Perl configuration; separate objects built
    here were never linked -- every file was compiled twice.)"""
    return Glob("src/languages/*.cxx")


def build_main_executable(env, languages):
    """Build the main PluMA executable."""
    program_libs = ["pthread", "m", "dl", "crypt", "c", "util"]
    # Only the languages compiled in: a disabled language's library need not
    # be installed.
    if env.get("PYTHON_ENABLED"):
        program_libs.append(f"python{python_version}")
    if env.get("PERL_ENABLED"):
        program_libs.append("perl")
    if env.get("R_ENABLED"):
        program_libs += ["R", "RInside"]
    if env.get("JAVA_ENABLED"):
        program_libs.append("jvm")
    if env.get("JULIA_ENABLED"):
        program_libs.append("julia")

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

    python_enabled = configure_python(config)
    perl_enabled = configure_perl(config)
    r_enabled = configure_r(config)
    configure_rust(config)
    julia_enabled = configure_julia(config)
    java_enabled = configure_java(config)

    config.Finish()

    # Strip annobin/hardened specs again — ParseConfig calls during
    # configure_perl / configure_python / configure_r may have
    # re-injected them after the initial strip in create_base_environment.
    _remove_annobin_flag(env)
    _strip_lto_flags(env)

    env["PYTHON_ENABLED"] = python_enabled
    env["PERL_ENABLED"] = perl_enabled
    env["R_ENABLED"] = r_enabled
    env["JAVA_ENABLED"] = java_enabled
    env["JULIA_ENABLED"] = julia_enabled
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

    modules = generate_swig_wrappers(env)
    build_language_bindings(env, modules)

    # (SHLIBPREFIX is already "lib": appending it again, as before, built
    # liblib<Name>Plugin.so, which PluMA never finds.)
    plugin_path = glob("./plugins/*/")

    # The CUDA flags before any plugin SConscript gets its copy of the CUDA
    # environment: PluMA's headers and the C++ standard.
    # cudaThreadSynchronize() was removed in CUDA 13: the same call as
    # cudaDeviceSynchronize(), so plugins written for older toolkits build.
    if env_cuda:
        env_cuda.AppendUnique(NVCCFLAGS=[f"-I{os.getcwd()}/src", CUDA_CXX_STANDARD,
                                         "-DcudaThreadSynchronize=cudaDeviceSynchronize"])
        global plugin_env_cuda
        plugin_env_cuda = env_cuda

    build_cpp_plugins(env, plugin_path)

    if env_cuda:
        build_cuda_plugins(env_cuda, plugin_path)

    if GetOption("with-rust") or GetOption("build-rust-plugins"):
        print(">> Compiling Rust Plugins")
        build_rust_plugins(env, plugin_path, release_mode=GetOption("rust-release"),
                           features=GetOption("rust-features") or "")

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
