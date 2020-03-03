#!python
# Copyright (C) 2017-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor configuration.
#
# TODO: Test portability with Windows systems using MSVC.
# TODO: Merge variable assignments.
import logging
from os import environ, getenv
from os.path import relpath
import re
import subprocess
import sys

from build_config import *
from build_support import *

# The current Sconstruct does not support windows variants.
if sys.platform.startswith("windows"):
    logging.error("Windows is not currently supported for compilation")
    Exit(1)

###################################################################
# Add Commndline Options to our Scons build
#
# We assume with-* options to be False for common plugins UNLESS the enduser
# specifies True. For CUDA, we assume False since CUDA availability is not
# expected on most end-user systems.

AddOption(
    "--prefix",
    dest="prefix",
    default="/usr/local",
    help="PREFIX for local installations",
)

AddOption(
    "--with-cuda",
    dest="with-cuda",
    action="store_true",
    default=False,
    help="Enable CUDA plugin compilation",
)

AddOption(
    "--rcpp-include",
    dest="rccp_include",
    default="/usr/local/lib/R/site-library/Rccp/includes",
    help="Location of the installed Rccp headers",
)

AddOption(
    "--rinside-include",
    dest="rinside_include",
    default="/usr/local/lib/R/site-library/RInside/include",
    help="Location of the installed RInside headers",
)

# AddOption(
#     "--without-python",
#     dest="without-python",
#     action="store_true",
#     default=False,
#     help="Disable Python plugin compilation",
# )
#
# AddOption(
#     "--without-perl",
#     dest="without-perl",
#     action="store_true",
#     default=False,
#     help="Disable Perl plugin compilation",
# )
#
# AddOption(
#     "--without-r",
#     dest="without-r",
#     action="store_true",
#     default=False,
#     help="Disable R plugin compilation",
# )

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.
###################################################################
env = Environment(
    ENV=environ,
    CC=getenv("CC", "cc"),
    CFLAGS=getenv("CFLAGS", "-std=c11"),
    CXX=getenv("CXX", "c++"),
    CXXFLAGS=getenv("CXXFLAGS", "-std=c++11"),
    LDFLAGS=getenv("LDFLAGS", "-D_FORTIFY_SOURCE=2"),
    SHCFLAGS="-fpermissive -Isrc/",
    SHCXXFLAGS="-fpermissive -Isrc/",
)

if not sys.platform.startswith("darwin"):
    env.Append(LINKFLAGS=["-rdynamic"])
###################################################################

###################################################################
# Either clean folders from previous Scons runs or begin assembling
# `PluMA` and its associated plugins
if env.GetOption("clean"):
    env.Clean(
        "python",
        [
            Glob("./**/__pycache__"),
            ObjectPath("PyPluMA.o"),
            ObjectPath("PyPluMA_wrap.o"),
            OutPath("_PyPluMA.so"),
        ],
    )

    env.Clean(
        "scons",
        [
            relpath(".sconf_temp"),
            relpath(".sconsign.dblite"),
            relpath("config.log"),
            OutPath("."),
            ObjectPath("."),
        ],
    )

    env.Clean("all", ["scons", "python"])
else:
    envPluginCuda = None

    ###################################################################
    # Check for headers, libraries, and build sub-environments
    # for plugins.
    ###################################################################
    config = Configure(env)

    if env.get("debug", 0):
        env.Append(CPPDEFINES=["DEBUG", "_DEBUG"], CXXFLAGS=["-g"])
    else:
        env.Append(CPPDEFINES=["NDEBUG"], CXXFLAGS=["-O2"])

    config.env.AppendUnique(
        CPPPATH=include_search_path,
        CXXFLAGS=["-pipe", "-fPIC", "-fstack-protector"],
        LIBPATH=lib_search_path,
    )

    if not config.CheckCXX():
        logging.error("!! CXX compiler could not output a valid executable")
        Exit(1)

    if not config.CheckSHCXX():
        logging.error(
            "!! Shared CXX compiler could not output a valid executable"
        )
        Exit(1)

    if not config.CheckLibWithHeader("m", "math.h", "c"):
        logging.error("!! Math library not found")
        Exit(1)

    if not config.CheckLib("pthread"):
        logging.error("!! Could not find `pthread` library")
        Exit(1)

    if sys.version_info[0] == "3":
        logging.warning(
            "!! Version of Python <= Python2.7 are now EOL. Please update to Python3"
        )
        if not sys.platform.startswith("darwin"):
            config.env.AppendUnique(LIBS=["rt"])

    if not config.CheckProg("perl"):
        logging.error("!! Could not find a valid `perl` installation`")
        Exit(1)
    else:
        perl_include = cmdline('perl -e "print qq(@INC)"').split(" ")

        for include_dir in perl_include:
            if "core_perl" in include_dir:
                include_dir = include_dir + "/CORE"
            config.env.Append(CPPPATH=[include_dir])

        if not config.CheckHeader("EXTERN.h"):
            logging.error("!! Could not find `EXTERN.h`")
            Exit(1)

        config.env.Append(
            CXXFLAGS=["-fno-strict-aliasing"],
            LDFLAGS=[
                "-DLARGE_SOURCE" "-D_FILE_OFFSET_BITS=64",
                "-DHAVE_PERL",
                "-DREENTRANT",
            ],
        )

        if sys.platform.startswith("darwin"):
            config.env.Append(LIBS=["crypt", "nsl"])

    if not config.CheckProg("R") or not config.CheckProg("Rscript"):
        logging.error("!! Could not find a valid `R` installation`")
        Exit(1)
    else:
        config.env.ParseConfig(
            "pkg-config --cflags-only-I --libs-only-L --libs-only-l libR"
        )
        config.env.AppendUnique(
            LDFLAGS=["-Bsymbolic-functions", "-z,relro", "-DHAVE_R"]
        )
        site_library = subprocess.check_output(
            'Rscript -e ".Library"',
            universal_newlines=True,
            encoding="utf8",
            shell=True,
        )

        site_library = re.sub(r'\[\d\]\s"(.+)"', r"\1", site_library).rstrip()

        config.env.AppendUnique(
            CXXFLAGS=[
                "-fpermissive",
                "--param=ssp-buffer-size=4",
                "-Wformat",
                "-Wformat-security",
                "-Werror=format-security",
            ],
            LIBPATH=[
                site_library + "/RCpp/includes",
                site_library + "/RInside/include",
            ],
            LIBS=["R"],
        )

    env = config.Finish()

    envPluginCuda = Environment(
        ENV=os.environ,
        CUDA_PATH=[os.getenv("CUDA_PATH", "/usr/local/cuda")],
        NVCCFLAGS=[
            os.getenv(
                "NVCCFLAGS",
                [
                    "-I" + os.getcwd(),
                    "-arch=sm_30",
                    "--ptxas-options=-v",
                    "-std=c++11",
                    "-Xcompiler",
                    "-fPIC" "-I" + os.getcwd(),
                ],
            )
        ],
    )
    envPluginCuda.Tool("cuda")

    # Export `envPlugin` and `envPluginCUDA`
    Export("env")
    Export("envPluginCuda")

    ###################################################################
    # Execute compilation for our plugins.
    # Note: CUDA is already prepared from the initial environment setup.
    ###################################################################

    ###################################################################
    # PYTHON PLUGINS
    env.SharedObject(
        source=SourcePath("PluMA.cxx"), target=SourcePath("PyPluMA.o")
    )
    env.SharedObject(SourcePath("PyPluMA_wrap.cxx"))
    env.SharedLibrary(
        source=[SourcePath("PyPluMA.os"), SourcePath("PyPluMA_wrap.o")],
        target=SourcePath("_PyPluMA_wrap.so"),
    )
    ###################################################################

    ###################################################################
    # PERL PLUGINS
    env.SharedObject(
        source=SourcePath("PluMA.cxx"), target=SourcePath("PerlPluMA.o")
    )
    env.SharedObject(SourcePath("PerlPluMA_wrap.cxx"))
    env.SharedLibrary(
        source=[SourcePath("PerlPluMA_wrap.os"), SourcePath("PerlPluMA.o"),],
        target="PerlPluMA_wrap.so",
    )
    ###################################################################
    # R PLUGINS
    env.SharedObject(
        source=SourcePath("PluMA.cxx"), target=SourcePath("RPluMA.o")
    )
    env.SharedObject(
        source=SourcePath("RPluMA_wrap.cxx"),
        target=SourcePath("RPluMA_wrap.o"),
    )
    env.SharedLibrary(
        source=[SourcePath("RPluMA.o"), SourcePath("RPluMA_wrap.cxx")],
        target=SourcePath("RPluMA_wrap.so"),
    )
    ###################################################################

    ###################################################################
    # Finally, compile!
    # It is dangerous to go alone, take this: 𐃉
    ###################################################################

    ###################################################################
    # Assemble plugin path
    pluginPath = Glob(SourcePath("") + "plugins/**")
    ###################################################################

    ##################################################################
    # C++ Plugins
    for folder in pluginPath:
        envPlugin.AppendUnique(CCFLAGS=["-I" + folder])
        if GetOption("with-cuda"):
            envPluginCUDA.AppendUnique(NVCCFLAGS=["-I" + folder])
        sconscripts = Glob(folder + "/*/SConscript")
        pluginListCXX = Glob(folder + "/*/*.{cpp, cxx}")
        if len(pluginListCXX) != 0 and len(sconscripts) != 0:
            for sconscript in sconscripts:
                SConscript(sconscript, exports=toExport)
        curFolder = ""
        firstTime = True
        for plugin in pluginListCXX:
            if plugin.get_dir() != curFolder:  # New context
                if not firstTime:
                    if len(pluginName) == 0:
                        logging.warning(
                            "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING"
                            % folder
                        )
                    else:
                        envPlugin.SharedLibrary(pluginName, sourceFiles)
                curFolder = plugin.get_dir()
                pluginName = ""
                sourceFiles = []
            firstTime = False
            filename = plugin.get_path()
            if filename.endswith("Plugin.cpp"):
                pluginName = filename[0 : filename.find(".cpp")]
            sourceFiles.append(filename)
        if len(pluginName) == 0:
            logging.warning(
                "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING" % folder
            )
        else:
            envPlugin.SharedLibrary(pluginName, sourceFiles)
    ###################################################################

    ###################################################################
    # CUDA Plugins
    #
    # TODO: Compress if-else statements?
    if GetOption("with-cuda"):
        for folder in pluginpath:
            pluginsCUDA = Glob(folder + "/*/*.cu")
            curFolder = ""
            firstTime = True
            for plugin in pluginsCUDA:
                if plugin.get_dir() != curFolder:  # New context
                    if not firstTime:
                        if len(sharedPluginName) == 0:
                            logging.warning(
                                "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING"
                                % folder
                            )
                        else:
                            envPluginCUDA.SharedLibrary(
                                sharedPluginName, sourceFiles
                            )
                curFolder = plugin.get_dir()
                sharedPluginName = ""
                sourceFiles = []
                srcStr = ""
            firstTime = False
            filename = plugin.get_path()
            if filename.endswith("Plugin.cu"):
                name = filename.replace(str(plugin.get_dir()), "")
                sharedPluginName = (
                    str(plugin.get_dir()) + "/lib" + name[1 : name.find(".cu")]
                )
            sourceFiles.append(filename)
            srcStr += filename + " "
        if len(sharedPluginName) == 0:
            logging.warning(
                "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING" % folder
            )
        else:
            envPluginCUDA.Command(sharedPluginName, sourceFiles)

        # Repeat of C++ plugins?
        # Consider DRY-ing?
        curFolder = ""
        firstTime = True
        for plugin in pluginListCXX:
            if plugin.get_dir() != curFolder:  # New context
                if not firstTime:
                    if len(pluginName) == 0:
                        logging.warning(
                            "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING"
                            % folder
                        )
                    else:
                        envPlugin.SharedLibrary(pluginName, sourcefiles)
                curFolder = plugin.get_dir()
                pluginName = ""
                sourceFiles = []
            firstTime = False
            filename = plugin.get_path()
            if filename.endswith("Plugin.cpp"):
                pluginName = filename[0 : filename.find(".cpp")]
            sourceFiles.append(filename)
        if len(pluginName) == 0:
            logging.warning(
                "WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING", folder
            )
        else:
            envPlugin.SharedLibrary(pluginName, sourceFiles)
    ###################################################################
    # Main Executable & PluGen
    env.AppendUnique(LDFLAGS=["-I./src/languages", "-I./src/PluGen"])

    env.Program(OutPath("PluGen/plugen"), Glob(SourcePath("PluGen/*.cxx")))
    env.Program(
        OutPath("pluma"),
        [Glob(SourcePath("languages/*.cxx")), SourcePath("PluginManager.h")],
    )
    ###################################################################
