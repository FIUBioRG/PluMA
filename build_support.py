#!python
# Copyright (C) 2017, 2019-2020, 2026 FIUBioRG
# SPDX-License-Identifier: MIT

import os
import subprocess
from os import path
from subprocess import PIPE, Popen
import sys
import glob
import logging

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
