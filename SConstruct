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
    default="sm_35",
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

AddOption(
    "--with-rust",
    dest="with-rust",
    action="store_true",
    default=False,
    help="Enable experimental support for Rust language plugins"
)

AddOption(
    "--r-include-dir",
    dest="r-include-dir",
    action="store",
    default="/usr/local/lib/R/include",
    help="Set the include directory for the R installation on the system"
)

AddOption(
    "--r-lib-dir",
    dest="r-lib-dir",
    action="store",
    default="/usr/local/lib/R/lib",
    help="Set the lib directory for the R installation on the system"
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
    CCFLAGS=["-fpermissive", "-fPIC", "-I.", "-O2"],
    CXXFLAGS=["-std=c++11", "-fPIC", "-O2"],
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
        "c",
    ]

    for lib in libs:
        if not config.CheckLib(lib):
            Exit(1)

    config.CheckProg("swig")

    if not env.GetOption("without-python"):
        config.CheckProg("python3-config")
        config.CheckProg("python3")

        config.env.ParseConfig("/usr/bin/python3-config --includes --ldflags")
        config.env.Append(LIBS=["util"])

        if sys.version_info[0] == "2":
            logging.warning(
                "!! Version of Python <= Python3.0 are now EOL. Please update to Python3"
            )

    if not env.GetOption("without-perl"):
        config.CheckProg("perl")

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

            config.env.Append(CPPDEFINES=["-DWITH_PERL"])

    if not env.GetOption("without-r"):
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
                    "-fno-gnu-unique",
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

    if GetOption("with-rust"):
        config.CheckProg("rustc")
        config.CheckProg("cargo")

    config.Finish()

    if GetOption("with-cuda"):

        envPluginCuda = Environment(
            ENV=os.environ,
            CUDA_PATH=[getenv("CUDA_PATH", "/usr/local/cuda")],
            CUDA_SDK_PATH=[getenv("CUDA_SDK_PATH", "/usr/local/cuda")],
            NVCCFLAGS=[
                "-I" + os.getcwd(),
                "--ptxas-options=-v",
                "-std=c++11",
                "-Xcompiler",
                "-fPIC",
            ],
            GPU_ARCH=GetOption('gpu_arch'),
        )

        configCuda = Configure(envPluginCuda)
        configCuda.CheckProg("nvcc")
        configCuda.CheckHeader("cuda.h")
        configCuda.Finish()

    # Export `envPlugin` and `envPluginCUDA`
    Export("env")
    Export("envPluginCuda")

    ###################################################################
    # Regenerate wrappers for plugin languages
    ###################################################################
    if not env.GetOption("without-python"):
        env.Command(
            "PyPluMA",
            "src/PluginWrapper.i",
            "swig -python -c++ -module $TARGET -o ${TARGET}_wrap.cxx $SOURCE"
        )

    if not env.GetOption("without-perl"):
        env.Command(
            "PerlPluMA",
            "src/PluginWrapper.i",
            "swig -perl5 -c++ -module $TARGET -o ${TARGET}_wrap.cxx $SOURCE"
        )

    if not env.GetOption("without-r"):
        env.Command(
            "RPluMA",
            "src/PluginWrapper.i",
            "swig -r -c++ -module $TARGET -o ${TARGET}_wrap.cxx $SOURCE"
        )

    ###################################################################
    # Execute compilation for our plugins.
    # Note: CUDA is already prepared from the initial environment setup.
    ###################################################################
    env.SharedObject(
        source=SourcePath("PluMA.cxx"),
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

    ###################################################################
    # # C++ Plugins
    print("!! Compiling C++ Plugins")
    for folder in pluginPath:
        sconsScripts = Glob(folder + "/SConscript")
        pluginListCXX = Glob(folder + "/*Plugin.cpp")
        if len(sconsScripts) != 0:
            for sconsScript in sconsScripts:
                SConscript(sconsScripts, exports=toExport)
        for plugin in pluginListCXX:
            filesInPath = Glob(str(plugin.get_dir()) + "/*.cpp")
            pluginName = plugin.get_path()
            pluginName = pluginName.replace(".cpp", ".so")
            env.SharedLibrary(
                target=pluginName, source=filesInPath
            )

    ###################################################################
    #
    # ###################################################################
    if GetOption("with-cuda"):
        print("!! Compiling CUDA Plugins")
        envPluginCuda.AppendUnique(NVCCFLAGS=["-I"+os.getcwd()+"/src", '-std=c++11'])
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
                    "nvcc -o $TARGET -shared $NVCCFLAGS -arch=$GPU_ARCH $SOURCES"
                )

    ###################################################################
    # Main Executable & PluGen
    env.Append(
        LIBPATH=[LibPath(""),]
    )

    languages = Glob("src/languages/*.cxx")

    for language in languages:
        output = language.get_path().replace("src", "obj").replace(".cxx", ".os")
        if "Perl" in output:
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
                source=language,
                target=output,
            )
        else:
            env.StaticObject(
                source=language,
                target=output
            )

    plugenFiles = Glob(str(SourcePath("PluGen/*.cxx")))

    env.Program("PluGen/plugen", Glob("src/PluGen/*.cxx"))

    sourceFiles = Glob("src/*.cxx")

    env.Program(
        target="pluma",
        source=[
            SourcePath("main.cxx"),
            SourcePath("PluginManager.cxx"),
            languages,
        ],
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
    )
    ###################################################################
