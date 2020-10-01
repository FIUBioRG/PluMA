#!/usr/bin/env python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor/compiler configuration
#
# @TODO: Test portability with Windows systems using MSVC
# @TODO: Merge variable assignments
# @TODO: Test portability with Mac OSX
#
# -*-python-*-

from glob import glob
from os import environ, getenv
from os.path import relpath, abspath
import logging
import re
import subprocess
import sys

from build_config import *
from build_support import *

if sys.version_info.major < 3:
    logging.error("Python3 is required to run this Sconstruct")

# The current SConstruct does not support Windows variants
if sys.platform.startswith("windows"):
    logging.error("Windows is currently not supported")
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
    type="string",
    nargs=1,
    action="store",
    metavar="DIR",
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
    "--gpu-architecture",
    dest="gpu_arch",
    type="string",
    nargs=1,
    action="store",
    metavar="ARCH",
    default="sm_30",
    help="Specify the name of the class of NVIDIA 'virtual' GPU architecture for which the CUDA input files must be compiled"
)

AddOption(
    "--without-python",
    dest="without-python",
    action="store_true",
    default=False,
    help="Disable Python plugin compilation",
)

AddOption(
    "--without-perl",
    dest="without-perl",
    action="store_true",
    default=False,
    help="Disable Perl plugin compilation",
)

AddOption(
    "--without-r",
    dest="without-r",
    action="store_true",
    default=False,
    help="Disable R plugin compilation",
)

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.
###################################################################
###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.
###################################################################
env = Environment(
    ENV=environ,
    CC=getenv("CC", "cc"),
    CXX=getenv("CXX", "c++"),
    CPPDEFINES=["HAVE_PYTHON"],
    SHCCFLAGS=["-fpermissive", "-fPIC", "-I.", "-O2"],
    SHCXXFLAGS=["-std=c++11", "-fPIC", "-I.", "-O2"],
    CCFLAGS=["-fpermissive", "-fPIC", "-I."],
    CXXFLAGS=["-std=c++11", "-fPIC"],
    CPPPATH=include_search_path,
    LIBPATH=lib_search_path,
    LICENSE=["MIT"],
    SHLIBPREFIX=""
)

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
            relpath("pythonds"),
            Glob("*.pdf"),
            relpath("tmp"),
        ],
    )

    env.Clean("all", ["python", "default"])
else:
    envPluginCuda = None

    ###################################################################
    # Check for headers, libraries, and build sub-environments
    # for plugins.
    ###################################################################
    config = Configure(env, custom_tests={"CheckPerl": CheckPerl})

    if not config.CheckCC():
        Exit(1)

    if not config.CheckCXX():
        Exit(1)

    if not config.CheckSHCXX():
        Exit(1)

    libs = [
        "m",
        "pthread",
        "dl",
        "crypt",
        "pcre",
        "rt",
        "blas",
        "rt",
        "c",
    ]

    for lib in libs:
        if not config.CheckLib(lib):
            Exit(1)

    config.CheckProg("python3-config")
    config.CheckProg("python3")
    config.CheckProg("perl")

    config.env.ParseConfig("/usr/bin/python3-config --includes --ldflags")
    config.env.Append(LIBS=["util"])

    if sys.version_info[0] == "2":
        logging.warning(
            "!! Version of Python <= Python2.7 are now EOL. Please update to Python3"
        )

    if not config.CheckPerl():
        logging.error("!! Could not find a valid `perl` installation`")
        Exit(1)
    else:
        config.env.ParseConfig("perl -MExtUtils::Embed -e ccopts -e ldopts")

        if not config.CheckHeader("EXTERN.h"):
            logging.error("!! Could not find `EXTERN.h`")
            Exit(1)

        config.env.AppendUnique(
            CXXFLAGS=["-fno-strict-aliasing"],
            CPPDEFINES=[
                "LARGE_SOURCE",
                "_FILE_OFFSET_BITS=64",
                "HAVE_PERL",
                "REENTRANT",
            ],
        )

        if sys.platform.startswith("darwin"):
            config.env.Append(LIBS=["crypt", "nsl"])

    if not config.CheckProg("R") or not config.CheckProg("Rscript"):
        logging.error("!! Could not find a valid `R` installation`")
        Exit(1)
    else:
        config.env.ParseConfig("pkg-config --cflags-only-I --libs-only-L libR")
        config.env.AppendUnique(
            LDFLAGS=["-Bsymbolic-functions", "-z,relro"], CPPDEFINES=["HAVE_R"]
        )

        config.env.AppendUnique(
            CXXFLAGS=[
                "-fpermissive",
                "-fopenmp",
                "--param=ssp-buffer-size=4",
                "-Wformat",
                "-Wformat-security",
                "-Werror=format-security",
                "-Wl,--export-dynamic",
            ],
            CPPPATH=[
                Dir("/usr/lib/R/library/Rcpp/include"),
                Dir("/usr/lib/R/library/RInside/include"),
                Dir('/usr/lib/R/site-library/RInside/include'),
                Dir('/usr/lib/R/site-library/Rcpp/include'),
                Dir("/usr/local/lib/R/library/Rcpp/include"),
                Dir("/usr/local/lib/R/library/RInside/include"),
                Dir('/usr/local/lib/R/site-library/RInside/include'),
                Dir('/usr/local/lib/R/site-library/Rcpp/include'),
            ],
            LIBPATH=[
                Dir("/usr/lib/R/library/RInside/lib"),
                Dir('/usr/lib/R/site-library/RInside/lib'),
                Dir("/usr/local/lib/R/library/RInside/lib"),
                Dir('/usr/local/lib/R/site-library/RInside/lib'),
            ],
            LIBS=["R", "RInside"],
        )

    if not GetOption("without-perl"):
        config.env.Append(CPPDEFINES=["-DWITH_PERL"])

    if not GetOption("without-r"):
        config.env.Append(CPPDEFINES=["-DWITH_R"])

    config.Finish()

    if GetOption("with-cuda"):

        envPluginCuda = Environment(
            ENV=os.environ,
            CUDA_PATH=[os.getenv("CUDA_PATH", "/usr/local/cuda")],
            CUDA_SDK_PATH=[os.getenv("CUDA_SDK_PATH", "/usr/local/cuda")],
            NVCCFLAGS=[
                "-I" + os.getcwd(),
                # "-arch=sm_30",
                "--ptxas-options=-v",
                "-std=c++11",
                "-Xcompiler",
                "-fPIC",
            ],
            GPUARCH=GetOption('gpu_arch'),
        )

        # envPluginCuda.Tool('cuda')

        configCuda = Configure(envPluginCuda)
        configCuda.CheckProg('nvcc')
        configCuda.CheckHeader("cuda.h")
        configCuda.Finish()

    # Export `envPlugin` and `envPluginCUDA`
    Export("env")
    Export("envPluginCuda")

    ###################################################################
    # Execute compilation for our plugins.
    # Note: CUDA is already prepared from the initial environment setup.
    ###################################################################
    env.SharedObject(
        source="PluMA.cxx",
        target=ObjectPath("PluMA.os"),
    )
    ###################################################################
    # PYTHON PLUGINS
    env.SharedObject(
        source="PyPluMA_wrap.cxx",
        target=ObjectPath("PyPluMA_wrap.os"),
    )

    env.SharedLibrary(
        source=[
            ObjectPath("PyPluMA_wrap.os"),
            ObjectPath("PluMA.os"),
        ],
        target="_PyPluMA.so",
    )
    ###################################################################

    ###################################################################
    # PERL PLUGINS
    env.SharedObject(
        source="PerlPluMA_wrap.cxx",
        target=ObjectPath("PerlPluMA_wrap.os"),
    )

    env.SharedLibrary(
        source=[
            ObjectPath("PluMA.os"),
            ObjectPath("PerlPluMA_wrap.os"),
        ],
        target="PerlPluMA.so",
    )
    ###################################################################
    # R PLUGINS
    env.SharedObject(
        source="RPluMA_wrap.cxx",
        target=ObjectPath("RPluMA_wrap.os"),
    )
    env.SharedLibrary(
        source=[
            ObjectPath("PluMA.os"),
            ObjectPath("RPluMA_wrap.os"),
        ],
        target="RPluMA.so",
    )
    ###################################################################

    ###################################################################
    # Finally, compile!
    ###################################################################

    env.Append(SHLIBPREFIX="lib")

    ###################################################################
    # Assemble plugin path
    pluginPath = glob("./plugins/*/")
    ###################################################################

    # ##################################################################
    # # CUDA Plugins
    # #
    # # TODO: Compress if-else statements?
    # # C++ Plugins
    print("!! Compiling C++ Plugins")
    for folder in pluginPath:
        env.AppendUnique(CCFLAGS=["-I" + folder])
        sconscripts = Glob(folder + "/SConscript")
        pluginListCXX = Glob(folder + "/*.cpp")
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
                        env.SharedLibrary(pluginName, sourceFiles)
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
                env.SharedLibrary(
                    target=pluginName, source=sourceFiles
                )
    # ###################################################################
    #
    # ###################################################################
    if GetOption("with-cuda"):
        print("!! Compiling CUDA Plugins")
        envPluginCuda.AppendUnique(NVCCFLAGS=["-I"+os.getcwd(), '-std=c++11'])
        for folder in pluginPath:
            pluginListCU = Glob(folder+'/*Plugin.cu')
            for plugin in pluginListCU:
                pluginName = plugin.get_path()
                pluginName = pluginName.replace(str(plugin.get_dir())+"/", "")
                pluginName = pluginName.replace(".cu", ".so")
                envPluginCuda.Command(
                    str(plugin.get_dir()) + "/lib" + pluginName,
                    plugin,
                    "nvcc -o $TARGET -shared $NVCCFLAGS $SOURCE"
                )

    ###################################################################
    # Main Executable & PluGen
    env.Append(
        LIBPATH=[LibPath(""),]
    )

    env.StaticObject(
        source="languages/Compiled.cxx",
        target=ObjectPath("languages/Compiled.o"),
    )

    env.StaticObject(
        source="languages/Language.cxx",
        target=ObjectPath("languages/Language.o"),
    )

    env.StaticObject(
        source="languages/Py.cxx",
        target=ObjectPath("languages/Py.o"),
    )

    env.StaticObject(
        LDFLAGS=[
            [
                subprocess.check_output(
                    "perl -MExtUtils::Embed -e ldopts",
                    universal_newlines=True,
                    shell=True,
                    encoding="utf8",
                )
            ]
        ],
        source="languages/Perl.cxx",
        target=ObjectPath("languages/Perl.o"),
    )

    env.StaticObject(
        source="languages/R.cxx",
        target=ObjectPath("languages/R.o"),
    )

    env.StaticObject(
        source="PluginManager.cxx",
        target=ObjectPath("PluginManager.o"),
    )

    env.StaticObject(
        source="main.cxx",
        target=ObjectPath("main.o"),
    )

    env.Program("PluGen/plugen", Glob("./PluGen/*.cxx"))

    env.Program(
        target="pluma",
        LIBS=[
            "pthread",
            "m",
            "dl",
            "crypt",
            "c",
            "python" + python_version,
            "util",
            "perl",
            "R",
            "RInside",
        ],
        source=[
            ObjectPath("languages/Compiled.o"),
            ObjectPath("languages/Language.o"),
            ObjectPath("languages/Py.o"),
            ObjectPath("languages/Perl.o"),
            ObjectPath("languages/R.o"),
            ObjectPath("PluginManager.o"),
            ObjectPath("main.o"),
        ],
    )
    ###################################################################
