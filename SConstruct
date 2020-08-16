#!python3
# Copyright (C) 2017, 2019-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor/compiler configuration
#
# @TODO: Test portability with Windows systems using MSVC
# @TODO: Merge variable assignments
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
    CPPDEFINES=["_FORTIFY_SOURCE=2", "HAVE_PYTHON"],
    SHCCFLAGS=["-fpermissive", "-fPIC", "-Isrc/", "-I."],
    CCFLAGS=["-fpermissive", "-fPIC", "-Isrc/", "-I."],
    CXXFLAGS=["-std=c++11"],
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
            relpath("PluGen"),
            relpath("./obj"),
            relpath("./lib"),
            Glob("./src/*.o"),
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

    if env.get("debug", 0):
        env.AppendUnique(CPPDEFINES=["DEBUG", "_DEBUG"], CXXFLAGS=["-g"])
    else:
        env.AppendUnique(CPPDEFINES=["NDEBUG"], CXXFLAGS=["-O2"])

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
        # "util",
        "pcre",
        "rt",
        "blas",
        "rt",
        "c",
    ]

    for lib in libs:
        if not config.CheckLib(lib):
            Exit(1)

    config.env.ParseConfig("/usr/bin/python-config --includes --ldflags")
    config.env.Append(LIBS=["python" + python_version])
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
                "-fopenmp",
                "--param=ssp-buffer-size=4",
                "-Wformat",
                "-Wformat-security",
                "-Werror=format-security",
                "-Wl,--export-dynamic",
            ],
            CPPPATH=[
                Dir(site_library + "/Rcpp/include"),
                Dir(site_library + "/RInside/include"),
            ],
            LIBPATH=[Dir(site_library + "/RInside/lib")],
            LIBS=["R", "RInside"],
        )

    if platform_id == "ID=arch":
        if not config.CheckLib("python3.8"):
            Exit(1)

        if not config.CheckLib("perl"):
            Exit(1)

        if not config.CheckLib("R"):
            Exit(1)

        if not config.CheckLib("RInside"):
            Exit(1)

    if not GetOption("without-perl"):
        config.env.Append(CPPDEFINES=["-DWITH_PERL"])

    if not GetOption("without-r"):
        config.env.Append(CPPDEFINES=["-DWITH_R"])

    env = config.Finish()

    if GetOption("with-cuda"):

        envPluginCuda = Environment(
            ENV=os.environ,
            CC="nvcc",
            CXX="nvcc",
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
                        "-fPIC",
                    ],
                )
            ],
        )
        env.CheckBuilder(language="cuda")
        envPluginCuda.Tool("cuda")

   #  if GetOption("with-tcp-socket"):
   #      if not config.CheckLib("uv"):
   #          logging.error("Missing libuv headers")
   #          exit(1)
   #      config.env.Append(CPPDEFINES=["-DWITH_HEARTBEAT"])

    # Export `envPlugin` and `envPluginCUDA`
    Export("env")
    Export("envPluginCuda")

    ###################################################################
    # Execute compilation for our plugins.
    # Note: CUDA is already prepared from the initial environment setup.
    ###################################################################
    env.SharedObject(
        source="PluMA.cxx",
        target=ObjectPath("PluMA.o"),
    )
    ###################################################################
    # PYTHON PLUGINS
    env.SharedObject(
        source="PyPluMA_wrap.cxx",
        target=ObjectPath("PyPluMA_wrap.o"),
    )

    env.SharedLibrary(
        source=[
            ObjectPath("PluMA.o"),
            ObjectPath("PyPluMA_wrap.o"),
        ],
        target="_PyPluMA.so",
    )
    ###################################################################

    ###################################################################
    # PERL PLUGINS
    env.SharedObject(
        source="PerlPluMA_wrap.cxx",
        target=ObjectPath("PerlPluMA_wrap.o"),
    )

    env.SharedLibrary(
        source=[
            ObjectPath("PluMA.o"),
            ObjectPath("PerlPluMA_wrap.o"),
        ],
        target="PerlPluMA.so",
    )
    ###################################################################
    # R PLUGINS
    env.SharedObject(
        source="RPluMA_wrap.cxx",
        target=ObjectPath("RPluMA_wrap.o"),
    )
    env.SharedLibrary(
        source=[
            ObjectPath("PluMA.o"),
            ObjectPath("RPluMA_wrap.o"),
        ],
        target="RPluMA.so",
    )
    ###################################################################

    ###################################################################
    # Finally, compile!
    # It is dangerous to go alone, take this: 𐃉
    ###################################################################

    ###################################################################
    # Assemble plugin path
    pluginPath = glob("./plugins/*/")
    ###################################################################

    # ##################################################################
    # # C++ Plugins
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
    # # CUDA Plugins
    # #
    # # TODO: Compress if-else statements?
    if GetOption("with-cuda"):
        for folder in pluginpath:
            pluginsCUDA = Glob(folder + "/*.cu")
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
                        env.SharedLibrary(pluginName, sourcefiles)
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
            env.SharedLibrary(pluginName, sourceFiles)
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

    env.SharedObject(
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
        RPATH=[site_library + "/RInside/lib"],
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
