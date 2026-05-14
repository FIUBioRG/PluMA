#!python
# Copyright (C) 2017, 2019-2020, 2026 FIUBioRG
# SPDX-License-Identifier: MIT

"""
Build support utilities for PluMA SConstruct.

This module provides helper functions for:
  - Path resolution (source, object, library directories)
  - Python version detection
  - Perl configuration checking
  - R library detection (Rcpp, RInside)
  - Java JDK detection (covers AlmaLinux/RHEL, Debian/Ubuntu, Arch, macOS)
"""

import logging
import os
import subprocess
import sys
from dataclasses import dataclass, field
from os import path
from subprocess import PIPE, Popen
import sys
import glob
import logging
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

# Fallback R library search paths (including user library)
_R_FALLBACK_PATHS = [
    os.path.expanduser("~/R/library"),  # User R library (common on Linux)
    os.path.expanduser("~/R/x86_64-pc-linux-gnu-library"),  # Arch Linux user library
    "/usr/lib/R/library",
    "/usr/lib/R/site-library",
    "/usr/local/lib/R/library",
    "/usr/local/lib/R/site-library",
]

# Fallback Java installation paths (covers AlmaLinux/RHEL, Debian/Ubuntu, Arch, macOS)
_JAVA_FALLBACK_PATHS = [
    # AlmaLinux / RHEL / CentOS / Fedora
    "/usr/lib/jvm/java",
    "/usr/lib/jvm/jre",
    "/usr/lib/jvm/java-17-openjdk",
    "/usr/lib/jvm/java-11-openjdk",
    "/usr/lib/jvm/java-1.8.0-openjdk",
    "/usr/lib/jvm/java-21-openjdk",
    "/usr/lib/jvm/java-17-amazon-corretto",
    "/usr/lib/jvm/java-11-amazon-corretto",
    # Debian / Ubuntu
    "/usr/lib/jvm/default-java",
    "/usr/lib/jvm/java-17-openjdk-amd64",
    "/usr/lib/jvm/java-11-openjdk-amd64",
    "/usr/lib/jvm/java-8-openjdk-amd64",
    "/usr/lib/jvm/java-21-openjdk-amd64",
    "/usr/lib/jvm/java-17-openjdk-arm64",
    "/usr/lib/jvm/java-11-openjdk-arm64",
    # Arch Linux
    "/usr/lib/jvm/default",
    "/usr/lib/jvm/java-17-openjdk",
    "/usr/lib/jvm/java-11-openjdk",
    "/usr/lib/jvm/java-21-openjdk",
    # macOS (Homebrew and system)
    "/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home",
    "/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home",
    "/opt/homebrew/opt/openjdk@11/libexec/openjdk.jdk/Contents/Home",
    "/usr/local/opt/openjdk/libexec/openjdk.jdk/Contents/Home",
    "/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home",
    "/Library/Java/JavaVirtualMachines/temurin-11.jdk/Contents/Home",
    # Generic
    "/usr/java/latest",
    "/usr/java/default",
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
        # Include user R library path in environment
        env = os.environ.copy()
        user_lib = os.path.expanduser("~/R/library")
        if os.path.isdir(user_lib):
            existing = env.get("R_LIBS_USER", "")
            if existing:
                env["R_LIBS_USER"] = f"{user_lib}:{existing}"
            else:
                env["R_LIBS_USER"] = user_lib

        result = subprocess.check_output(
            ["Rscript", "-e", r_code],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
            env=env,
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
# Java Detection
# =============================================================================


@dataclass
class JavaConfig:
    """Configuration paths for Java JDK/JRE installation."""

    java_home: Optional[str] = None
    include_dir: Optional[str] = None
    platform_include_dir: Optional[str] = None
    lib_dir: Optional[str] = None
    libjvm_path: Optional[str] = None

    # Lists of all found paths
    include_paths: List[str] = field(default_factory=list)
    lib_paths: List[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        """Check if minimum Java configuration was detected."""
        return self.java_home is not None and self.libjvm_path is not None

    @property
    def platform(self) -> str:
        """Get the platform identifier for JNI headers."""
        if sys.platform.startswith("linux"):
            return "linux"
        elif sys.platform.startswith("darwin"):
            return "darwin"
        elif sys.platform.startswith("win"):
            return "win32"
        return sys.platform


def _detect_java_home_from_env() -> Optional[str]:
    """Detect JAVA_HOME from environment variable."""
    java_home = os.environ.get("JAVA_HOME")
    if java_home and os.path.isdir(java_home):
        return java_home
    return None


def _detect_java_home_from_javac() -> Optional[str]:
    """Detect JAVA_HOME by finding javac and following symlinks."""
    try:
        # Find javac
        javac_path = subprocess.check_output(
            ["which", "javac"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()

        if javac_path:
            # Resolve symlinks to get real path
            real_path = os.path.realpath(javac_path)
            # javac is typically in $JAVA_HOME/bin/javac
            bin_dir = os.path.dirname(real_path)
            java_home = os.path.dirname(bin_dir)

            if os.path.isdir(java_home):
                return java_home
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


def _detect_java_home_from_java() -> Optional[str]:
    """Detect JAVA_HOME by finding java and following symlinks."""
    try:
        java_path = subprocess.check_output(
            ["which", "java"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()

        if java_path:
            real_path = os.path.realpath(java_path)
            bin_dir = os.path.dirname(real_path)
            java_home = os.path.dirname(bin_dir)

            if os.path.isdir(java_home):
                return java_home
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


def _detect_java_home_from_fallback() -> Optional[str]:
    """Detect JAVA_HOME from common installation paths."""
    for fallback_path in _JAVA_FALLBACK_PATHS:
        if os.path.isdir(fallback_path):
            # Verify it looks like a valid Java installation
            if os.path.isdir(os.path.join(fallback_path, "include")) or \
               os.path.isdir(os.path.join(fallback_path, "lib")):
                return fallback_path
    return None


def _detect_java_home_alternatives() -> Optional[str]:
    """Detect JAVA_HOME using update-alternatives (Debian/Ubuntu/RHEL)."""
    try:
        # Try update-alternatives for java
        result = subprocess.check_output(
            ["update-alternatives", "--display", "java"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        )

        # Parse output to find the current best version
        for line in result.split('\n'):
            if line.strip().startswith('/') and 'jre/bin/java' in line or 'bin/java' in line:
                java_path = line.split()[0]
                real_path = os.path.realpath(java_path)
                # Go up from bin/java to JAVA_HOME
                java_home = os.path.dirname(os.path.dirname(real_path))
                if os.path.isdir(java_home):
                    return java_home
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


def _detect_java_home() -> Optional[str]:
    """
    Detect JAVA_HOME using multiple methods.

    Detection priority:
      1. JAVA_HOME environment variable
      2. Follow javac symlink
      3. Follow java symlink
      4. update-alternatives (Linux)
      5. Fallback paths
    """
    detectors = [
        _detect_java_home_from_env,
        _detect_java_home_from_javac,
        _detect_java_home_from_java,
        _detect_java_home_alternatives,
        _detect_java_home_from_fallback,
    ]

    for detector in detectors:
        java_home = detector()
        if java_home:
            return java_home

    return None


def _detect_java_include_dir(java_home: Optional[str]) -> Optional[str]:
    """Detect Java JNI include directory."""
    if not java_home:
        return None

    include_dir = os.path.join(java_home, "include")
    if os.path.isdir(include_dir) and os.path.isfile(os.path.join(include_dir, "jni.h")):
        return include_dir

    return None


def _detect_java_platform_include(java_home: Optional[str], platform: str) -> Optional[str]:
    """Detect platform-specific JNI include directory."""
    if not java_home:
        return None

    include_dir = os.path.join(java_home, "include")
    platform_dir = os.path.join(include_dir, platform)

    if os.path.isdir(platform_dir):
        return platform_dir

    return None


def _find_libjvm(java_home: Optional[str]) -> tuple:
    """
    Find libjvm library path.

    Returns:
        Tuple of (lib_dir, libjvm_path) or (None, None) if not found
    """
    if not java_home:
        return None, None

    # Common relative paths to libjvm
    lib_patterns = [
        # Linux server JVM (most common for JDK)
        "lib/server/libjvm.so",
        "jre/lib/server/libjvm.so",
        "lib/amd64/server/libjvm.so",
        "jre/lib/amd64/server/libjvm.so",
        "lib/aarch64/server/libjvm.so",
        "jre/lib/aarch64/server/libjvm.so",
        # Linux client JVM
        "lib/client/libjvm.so",
        "jre/lib/client/libjvm.so",
        # AlmaLinux / RHEL specific paths
        "lib/server/libjvm.so",
        "lib/jli/libjli.so",  # Sometimes needed
        # macOS
        "lib/server/libjvm.dylib",
        "jre/lib/server/libjvm.dylib",
        "lib/libjli.dylib",
        # Generic lib directory
        "lib/libjvm.so",
        "jre/lib/libjvm.so",
    ]

    for pattern in lib_patterns:
        libjvm_path = os.path.join(java_home, pattern)
        if os.path.isfile(libjvm_path):
            lib_dir = os.path.dirname(libjvm_path)
            return lib_dir, libjvm_path

    return None, None


def detect_java_config() -> JavaConfig:
    """
    Automatically detect Java JDK/JRE configuration.

    Detection priority:
      1. JAVA_HOME environment variable
      2. Follow javac/java symlinks
      3. update-alternatives (Linux)
      4. Fallback to common installation paths

    Covers:
      - AlmaLinux / RHEL / CentOS / Fedora
      - Debian / Ubuntu
      - Arch Linux
      - macOS (Homebrew and system Java)

    Returns:
        JavaConfig dataclass with detected paths
    """
    config = JavaConfig()

    # Detect JAVA_HOME
    config.java_home = _detect_java_home()

    if not config.java_home:
        logging.warning("Could not detect JAVA_HOME")
        return config

    # Detect include directories
    config.include_dir = _detect_java_include_dir(config.java_home)
    config.platform_include_dir = _detect_java_platform_include(
        config.java_home, config.platform
    )

    # Find libjvm
    config.lib_dir, config.libjvm_path = _find_libjvm(config.java_home)

    # Collect all include paths
    if config.include_dir:
        config.include_paths.append(config.include_dir)
    if config.platform_include_dir:
        config.include_paths.append(config.platform_include_dir)

    # Collect all lib paths
    if config.lib_dir:
        config.lib_paths.append(config.lib_dir)

    _log_java_detection_results(config)

    return config


def _log_java_detection_results(config: JavaConfig) -> None:
    """Log Java detection results for debugging."""
    if config.java_home:
        logging.info(f"Java home detected: {config.java_home}")
    if config.include_dir:
        logging.info(f"Java include detected: {config.include_dir}")
    if config.platform_include_dir:
        logging.info(f"Java platform include detected: {config.platform_include_dir}")
    if config.lib_dir:
        logging.info(f"Java lib detected: {config.lib_dir}")
    if config.libjvm_path:
        logging.info(f"libjvm detected: {config.libjvm_path}")


def CheckJava(ctx) -> bool:
    """
    SCons custom test to verify Java JDK configuration.

    Args:
        ctx: SCons Configure context

    Returns:
        True if Java JDK is properly configured, False otherwise
    """
    ctx.Message("Checking Java JDK configuration... ")

    config = detect_java_config()

    if not config.is_valid:
        if not config.java_home:
            ctx.Result("JAVA_HOME not found")
        elif not config.libjvm_path:
            ctx.Result("libjvm not found")
        else:
            ctx.Result("incomplete configuration")
        return False

    ctx.Result(f"found ({config.java_home})")
    return True


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

    return retcode == 0


###################################################################
# JAVA DETECTION
###################################################################

def _is_rhel_like():
    """Check if the system is RHEL-like (CentOS, AlmaLinux, Rocky Linux, Fedora, etc.)."""
    try:
        if os.path.isfile("/etc/os-release"):
            with open("/etc/os-release", "r") as f:
                content = f.read().lower()
                rhel_indicators = [
                    "rhel", "centos", "almalinux", "rocky", "fedora",
                    "oracle linux", "scientific linux", "amazon linux"
                ]
                return any(indicator in content for indicator in rhel_indicators)
        # Also check for /etc/redhat-release
        if os.path.isfile("/etc/redhat-release"):
            return True
    except (IOError, OSError):
        pass
    return False


class JavaConfig:
    """Configuration holder for Java installation details."""
    def __init__(self):
        self.java_home = None
        self.include_paths = []
        self.lib_paths = []
        self.libjvm_path = None
        self.is_valid = False
        self.version = None

    def __repr__(self):
        return f"JavaConfig(java_home={self.java_home}, valid={self.is_valid})"


def detect_java_config():
    """
    Detect Java installation and return JavaConfig with paths.

    Detection order:
    1. JAVA_HOME environment variable
    2. Location of javac executable
    3. Common installation directories (including RHEL/CentOS/AlmaLinux alternatives)

    Note for RHEL/CentOS/AlmaLinux/Rocky Linux users:
        You need the JDK development package installed, not just the JRE.
        Install with: sudo dnf install java-11-openjdk-devel
        (or java-17-openjdk-devel, java-21-openjdk-devel for newer versions)

    Returns:
        JavaConfig: Configuration object with detected paths
    """
    config = JavaConfig()

    # Step 1: Check JAVA_HOME
    java_home = os.environ.get("JAVA_HOME")

    # Step 2: Try to find from javac location
    if not java_home or not os.path.isdir(java_home):
        java_home = _find_java_home_from_javac()

    # Step 3: Search common installation directories
    if not java_home or not os.path.isdir(java_home):
        java_home = _search_common_java_locations()

    if not java_home or not os.path.isdir(java_home):
        # Check if we're on RHEL-like system and provide helpful message
        if _is_rhel_like():
            logging.warning(
                "Java installation not found. On RHEL/CentOS/AlmaLinux, ensure the "
                "JDK development package is installed: sudo dnf install java-11-openjdk-devel"
            )
        else:
            logging.warning("Java installation not found")
        return config

    config.java_home = java_home

    # Determine platform-specific include subdirectory
    if sys.platform.startswith("linux"):
        platform_dir = "linux"
    elif sys.platform.startswith("darwin"):
        platform_dir = "darwin"
    elif sys.platform.startswith("win"):
        platform_dir = "win32"
    else:
        platform_dir = sys.platform

    # Configure include paths
    include_dir = os.path.join(java_home, "include")
    if os.path.isdir(include_dir):
        config.include_paths.append(include_dir)
        platform_include = os.path.join(include_dir, platform_dir)
        if os.path.isdir(platform_include):
            config.include_paths.append(platform_include)

    # Configure library paths and find libjvm
    lib_paths_to_check = [
        os.path.join(java_home, "lib", "server"),
        os.path.join(java_home, "lib", "client"),
        os.path.join(java_home, "lib"),
        os.path.join(java_home, "jre", "lib", "server"),
        os.path.join(java_home, "jre", "lib", "amd64", "server"),
        os.path.join(java_home, "jre", "lib", "i386", "server"),
    ]

    # macOS-specific paths
    if sys.platform.startswith("darwin"):
        lib_paths_to_check.extend([
            os.path.join(java_home, "lib", "jli"),
            os.path.join(java_home, "jre", "lib", "jli"),
        ])

    for lib_path in lib_paths_to_check:
        if os.path.isdir(lib_path):
            config.lib_paths.append(lib_path)
            # Check for libjvm
            if sys.platform.startswith("win"):
                libjvm = os.path.join(lib_path, "jvm.dll")
            elif sys.platform.startswith("darwin"):
                libjvm = os.path.join(lib_path, "libjvm.dylib")
            else:
                libjvm = os.path.join(lib_path, "libjvm.so")

            if os.path.isfile(libjvm):
                config.libjvm_path = libjvm

    # Validate: we need include paths and libjvm
    jni_header = os.path.join(include_dir, "jni.h") if include_dir else None
    if config.include_paths and (config.libjvm_path or config.lib_paths):
        if jni_header and os.path.isfile(jni_header):
            config.is_valid = True

    # Try to get Java version
    config.version = _get_java_version(java_home)

    if config.is_valid:
        logging.info(f"Java detected: {java_home} (version: {config.version})")
    else:
        # Provide helpful error message
        missing = []
        if not config.include_paths or not (jni_header and os.path.isfile(jni_header)):
            missing.append("JNI headers (jni.h)")
        if not config.libjvm_path and not config.lib_paths:
            missing.append("libjvm")

        msg = f"Java installation at {java_home} is incomplete (missing: {', '.join(missing)})"

        if _is_rhel_like() and "JNI headers" in missing[0] if missing else False:
            msg += ". Install the JDK development package: sudo dnf install java-11-openjdk-devel"

        logging.warning(msg)

    return config


def _find_java_home_from_javac():
    """Find JAVA_HOME by locating the javac executable."""
    try:
        # Try 'which javac' on Unix-like systems
        if not sys.platform.startswith("win"):
            result = subprocess.run(
                ["which", "javac"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                javac_path = result.stdout.strip()
                if javac_path:
                    # Resolve symlinks (important for alternatives system on RHEL/CentOS/AlmaLinux)
                    real_path = os.path.realpath(javac_path)
                    # javac is typically in JAVA_HOME/bin/javac
                    java_home = os.path.dirname(os.path.dirname(real_path))

                    # On RHEL-like systems, javac might resolve to something like:
                    # /usr/lib/jvm/java-11-openjdk-11.0.x.x-x.el8.x86_64/bin/javac
                    # Verify this is a valid JDK with headers
                    include_dir = os.path.join(java_home, "include")
                    if os.path.isdir(java_home) and os.path.isdir(include_dir):
                        return java_home

                    # If we didn't find include dir, the resolved path might be JRE
                    # Try to find corresponding JDK by looking at parent structure
                    # For RHEL-like: /usr/lib/jvm/java-X-openjdk-VERSION
                    parent = os.path.dirname(java_home)
                    if parent and os.path.basename(parent) == "jvm":
                        # Look for matching JDK in /usr/lib/jvm
                        import re
                        base_name = os.path.basename(java_home)
                        # Extract java version pattern
                        match = re.match(r'(java-\d+)', base_name)
                        if match:
                            version_prefix = match.group(1)
                            for item in os.listdir(parent):
                                if item.startswith(version_prefix) and "openjdk" in item:
                                    candidate = os.path.join(parent, item)
                                    candidate_include = os.path.join(candidate, "include")
                                    if os.path.isdir(candidate_include):
                                        return candidate
        else:
            # Windows: try 'where javac'
            result = subprocess.run(
                ["where", "javac"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                javac_path = result.stdout.strip().split('\n')[0]
                if javac_path:
                    java_home = os.path.dirname(os.path.dirname(javac_path))
                    if os.path.isdir(java_home):
                        return java_home
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        pass

    return None


def _search_common_java_locations():
    """Search common Java installation directories."""
    search_patterns = []

    if sys.platform.startswith("linux"):
        # Check for RHEL/CentOS/AlmaLinux alternatives symlink first
        # This is the most reliable way on these systems
        alternatives_paths = [
            "/etc/alternatives/java_sdk",           # RHEL/CentOS/AlmaLinux JDK
            "/etc/alternatives/java_sdk_openjdk",   # OpenJDK specific
            "/etc/alternatives/jre_openjdk",        # JRE (fallback)
        ]
        for alt_path in alternatives_paths:
            if os.path.islink(alt_path):
                real_path = os.path.realpath(alt_path)
                if os.path.isdir(real_path):
                    # For JRE paths, try to find corresponding JDK
                    if "jre" in real_path and "jdk" not in real_path:
                        # Try parent or sibling JDK directory
                        jdk_path = real_path.replace("jre", "jdk")
                        if os.path.isdir(jdk_path):
                            real_path = jdk_path
                    include_dir = os.path.join(real_path, "include")
                    if os.path.isdir(include_dir) and os.path.isfile(os.path.join(include_dir, "jni.h")):
                        return real_path

        search_patterns = [
            # RHEL/CentOS/AlmaLinux/Rocky Linux patterns (with arch suffix)
            "/usr/lib/jvm/java-*-openjdk-*",
            "/usr/lib/jvm/java-openjdk",
            # Standard OpenJDK patterns
            "/usr/lib/jvm/java-*-openjdk",
            "/usr/lib/jvm/java-*-oracle*",
            "/usr/lib/jvm/jdk-*",
            "/usr/lib/jvm/java-*",
            "/usr/lib/jvm/default-java",
            "/usr/lib/jvm/default",
            # Amazon Corretto
            "/usr/lib/jvm/java-*-amazon-corretto*",
            # Temurin/Adoptium
            "/usr/lib/jvm/temurin-*",
            # Oracle/manual installs
            "/usr/java/jdk*",
            "/usr/java/latest",
            "/usr/java/default",
            "/opt/java/jdk*",
            "/opt/jdk*",
            # Fedora patterns
            "/usr/lib/jvm/java",
        ]
    elif sys.platform.startswith("darwin"):
        search_patterns = [
            "/Library/Java/JavaVirtualMachines/*/Contents/Home",
            "/System/Library/Java/JavaVirtualMachines/*/Contents/Home",
            "/usr/local/opt/openjdk*/libexec/openjdk.jdk/Contents/Home",
            "/opt/homebrew/opt/openjdk*/libexec/openjdk.jdk/Contents/Home",
            # Temurin/Adoptium on macOS
            "/Library/Java/JavaVirtualMachines/temurin-*/Contents/Home",
            # Amazon Corretto on macOS
            "/Library/Java/JavaVirtualMachines/amazon-corretto-*/Contents/Home",
        ]
    elif sys.platform.startswith("win"):
        search_patterns = [
            "C:/Program Files/Java/jdk*",
            "C:/Program Files (x86)/Java/jdk*",
            "C:/Program Files/Eclipse Adoptium/jdk*",
            "C:/Program Files/Microsoft/jdk*",
            "C:/Program Files/Amazon Corretto/*",
            "C:/Program Files/Zulu/*",
        ]

    candidates = []
    for pattern in search_patterns:
        matches = glob.glob(pattern)
        candidates.extend(matches)

    # Sort candidates to prefer:
    # 1. JDK over JRE
    # 2. Newer versions over older
    def sort_key(path):
        is_jdk = "jdk" in path.lower() or "java_sdk" in path.lower()
        # Extract version number for sorting
        version = 0
        import re
        match = re.search(r'(\d+)(?:\.(\d+))?', path)
        if match:
            major = int(match.group(1))
            minor = int(match.group(2)) if match.group(2) else 0
            version = major * 1000 + minor
        return (is_jdk, version)

    candidates.sort(key=sort_key, reverse=True)

    # Return first valid candidate with include directory
    for candidate in candidates:
        include_dir = os.path.join(candidate, "include")
        if os.path.isdir(include_dir) and os.path.isfile(os.path.join(include_dir, "jni.h")):
            return candidate

    return None


def _get_java_version(java_home):
    """Get Java version from the installation."""
    try:
        java_bin = os.path.join(java_home, "bin", "java")
        if sys.platform.startswith("win"):
            java_bin += ".exe"

        if os.path.isfile(java_bin):
            result = subprocess.run(
                [java_bin, "-version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            # Java outputs version to stderr
            output = result.stderr or result.stdout
            if output:
                # Parse version from output like: openjdk version "11.0.11" or java version "1.8.0_291"
                for line in output.split('\n'):
                    if 'version' in line.lower():
                        parts = line.split('"')
                        if len(parts) >= 2:
                            return parts[1]
    except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
        pass

    return "unknown"
__all__ = [
    "python_version",
    "SourcePath",
    "ObjectPath",
    "LibPath",
    "cmdline",
    "get_perl_ldopts",
    "CheckPerl",
    "CheckRPackages",
    "CheckJava",
    "RConfig",
    "detect_r_config",
    "check_r_packages",
    "JavaConfig",
    "detect_java_config",
]
