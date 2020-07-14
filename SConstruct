#!python
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

# The current SConstruct does not support Windows variants
if sys.platform.startswith("windows"):
    logging.error("Windowns is currently not supported")
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
    SHCCFLAGS=["-fpermissive", "-fPIC", "-Isrc/"],
    CPPPATH=include_search_path,
    LIBPATH=lib_search_path,
    LICENSE=["MIT"],
)

if not sys.platform.startswith("darwin"):
    env.Append(LINKFLAGS=["-rdynamic"])
    env.Append(LIBS=["rt"])
else:
    env.Append(CCFLAGS=['-DAPPLE'])

if platform_id == "alpine":
    env.Append(CPPDEFINES=["__MUSL__"])

# envPlugin.Append(SHCCFLAGS = '-fpermissive')
# envPlugin.Append(SHCCFLAGS = '-I'+os.environ['PWD'])
# envPlugin.Append(SHCCFLAGS = '-std=c++0x')
#envPlugin.Append(LIBS = ['gsl'])
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
        source=SourcePath("plugin/PluMA.cxx"),
        target=ObjectPath("plugin/PluMA.o"),
    )
    ###################################################################
    # PYTHON PLUGINS
    env.SharedObject(
        source=SourcePath("plugin/PyPluMA_wrap.cxx"),
        target=ObjectPath("plugin/PyPluMA_wrap.o"),
    )
    env.SharedLibrary(
        source=[
            ObjectPath("plugin/PluMA.o"),
            ObjectPath("plugin/PyPluMA_wrap.o"),
        ],
        target=LibPath("_PyPluMA_wrap.so"),
    )
    ###################################################################

    ###################################################################
    # PERL PLUGINS
    env.SharedObject(
        source=SourcePath("plugin/PerlPluMA_wrap.cxx"),
        target=ObjectPath("plugin/PerlPluMA_wrap.o"),
    )
    env.SharedLibrary(
        source=[
            ObjectPath("plugin/PluMA.o"),
            ObjectPath("plugin/PerlPluMA_wrap.o"),
        ],
        target=LibPath("PerlPluMA_wrap.so"),
    )
    ###################################################################
    # R PLUGINS
    env.SharedObject(
        source=SourcePath("plugin/RPluMA_wrap.cxx"),
        target=ObjectPath("plugin/RPluMA_wrap.o"),
    )
    env.SharedLibrary(
        source=[
            ObjectPath("plugin/PluMA.o"),
            ObjectPath("plugin/RPluMA_wrap.o"),
        ],
        target=LibPath("RPluMA_wrap.so"),
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
        # if GetOption("with-cuda"):
        #     envPluginCUDA.AppendUnique(NVCCFLAGS=["-I" + folder])
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
                envPlugin.SharedLibrary(
                    target=ObjectPath(pluginName), source=sourceFiles
                )
    # ###################################################################
    #
    # ###################################################################
    # # CUDA Plugins
    # #
    # # TODO: Compress if-else statements?
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
    env.Append(
        LIBPATH=[LibPath(""),]
    )

    env.StaticObject(
        source=SourcePath("languages/Compiled.cxx"),
        target=ObjectPath("languages/Compiled.o"),
    )

    env.StaticObject(
        source=SourcePath("languages/Language.cxx"),
        target=ObjectPath("languages/Language.o"),
    )

    env.StaticObject(
        source=SourcePath("languages/Py.cxx"),
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
        source=SourcePath("languages/Perl.cxx"),
        target=ObjectPath("languages/Perl.o"),
    )

    env.StaticObject(
        source=SourcePath("languages/R.cxx"),
        target=ObjectPath("languages/R.o"),
    )

    env.StaticObject(
        source=SourcePath("main.cxx"), target=ObjectPath("main.o"),
    )

    env.Program("PluGen/plugen", Glob("./src/PluGen/*.cxx"))
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
            ObjectPath("main.o"),
        ],
    )
    ###################################################################

# ###################################################################
# # PYTHON INFORMATION
# python_dir = sys.exec_prefix
# python_version = sys.version[0:3]
# python_include = '-I'+getEnvVar('PYTHON_INCLUDE_DIR', python_dir+'/include/python'+python_version)
# python_lib = getEnvVar('PYTHON_LIB_DIR', python_dir+'/lib/python'+python_version+'/config')
# if (python == 1):
#    env.Append(CCFLAGS = python_include)
#    env.Append(LIBPATH = [python_lib])
#    env.Append(LIBS = ['python'+python_version])
#    env.Append(CCFLAGS = '-DHAVE_PYTHON')
#    env.Append(LIBS = ['pthread'])
#    env.Append(LIBS = ['util'])
#    if (python_version[0] == '3' and env['PLATFORM'] != 'darwin'):
#       env.Append(LIBS = ['rt'])
#    # Interface
#    if not env.GetOption('clean'):
#       if (env['PLATFORM'] == 'darwin'):
#          os.system("make -f Makefile.darwin.interface PYTHON_VERSION="+python_version+" PYTHON_LIB="+python_lib+" PYTHON_INCLUDE="+python_include+" python")
#       else:
#          os.system("make -f Makefile.interface PYTHON_INCLUDE="+python_include+" python")
#    else:
#      if (env['PLATFORM'] == 'darwin'):
#         os.system("make -f Makefile.darwin.interface pythonclean")
#      else:
#         os.system("make -f Makefile.interface pythonclean")
# ###################################################################


# ###################################################################
# # R (IF INSTALLED)
# if (r==1):
#    # Get the installation directory of R
#    r_lib = getEnvVar('R_LIB_DIR', '/usr/local/lib/R/')
#    r_include = getEnvVar('R_INCLUDE_DIR', '/usr/share/R/include/')
#    rinside_lib = getEnvVar('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
#    rinside_include = getEnvVar('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
#    rcpp_include = getEnvVar('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')

#    env.Append(CCFLAGS = '-I'+r_include)
#    env.Append(CCFLAGS = '-I'+rinside_include)
#    env.Append(CCFLAGS = '-I'+rcpp_include)
#    env.Append(LIBPATH = [r_lib])
#    env.Append(LIBPATH = [rinside_lib])
#    env.Append(CCFLAGS = '-DHAVE_R')
#    env.Append(LIBS = ['R'])
#    env.Append(LIBS = ['RInside'])
#    if not env.GetOption('clean'):
#      if (env['PLATFORM'] == 'darwin'):
#         os.system("make -f Makefile.darwin.interface R_INCLUDE=-I"+r_include+" R_LIB=-L"+r_lib+" r")
#      else:
#         os.system("make -f Makefile.interface R_INCLUDE=-I"+r_include+" R_LIB=-L"+r_lib+" r")
#    else:
#      if (env['PLATFORM'] == 'darwin'):
#         os.system("make -f Makefile.darwin.interface rclean")
#      else:
#         os.system("make -f Makefile.interface rclean")
# ###################################################################

# ###################################################################
# if (docuda==1):
#    envPluginCUDA = Environment(ENV = os.environ)
#    envPluginCUDA.Tool('cuda')
#    envPluginCUDA2 = Environment(ENV = os.environ)
#    envPluginCUDA.Tool('cuda')
#    envPluginCUDA.Append(NVCCFLAGS = '-I'+os.environ['PWD'])
#    envPluginCUDA.Append(NVCCFLAGS = ['-arch=sm_30'])
#    envPluginCUDA.Append(NVCCFLAGS = ['--ptxas-options=-v'])
#    envPluginCUDA.Append(NVCCFLAGS = ['-std=c++11'])
#    envPluginCUDA.Append(NVCCFLAGS = ['-Xcompiler'])
#    envPluginCUDA.Append(NVCCFLAGS = ['-fpic'])
#    envPluginCUDA2['NVCCFLAGS'] = ['-shared']
# ###################################################################

# ###################################################################
# if (perl==1):
#    # Get the installation directory of Perl
#    # Note: Will assume /usr/ if PERLHOME is unset
#    # Also need pthreads, assuming /usr/local is PTHREADHOME is unset
#    #perlhome = getEnvVar('PERLHOME', '/usr/')
#    pthread_lib = getEnvVar('PTHREAD_LIB_DIR', '/usr/local/lib')
#    pthread_include = getEnvVar('PTHREAD_LIB_DIR', '/usr/local/include')
#    getversion = str(subprocess.check_output(['perl', '-V'])).split(' ')
#    perlversion = getversion[5]+'.'+getversion[7]
#    perlarch = getEnvVar('PERLARCH', '')
#    if (env['PLATFORM'] != 'darwin'):
#       perl_include = getEnvVar('PERL_INCLUDE_DIR', '/usr/lib/perl/'+perlversion+'/CORE')
#    else:
#       perl_include = getEnvVar('PERL_INCLUDE_DIR', '/usr/perl'+getversion[5]+'/'+perlversion+'.0/'+perlarch+'/CORE')
#    perl_lib = getEnvVar('PERL_LIB_DIR', perl_include)

#    env.Append(LIBPATH = [perl_lib])
#    env.Append(CCFLAGS = '-I'+perl_include)
#    env.Append(CCFLAGS = '-DHAVE_PERL')
#    #env.Append(LIBPATH = [perl_lib])
#    env.Append(CCFLAGS = '-Wl,-E')
#    env.Append(CCFLAGS = '-fstack-protector')
#    env.Append(LIBPATH = [pthread_lib])
#    env.Append(LIBS = ['perl'])
#    env.Append(LIBS = ['dl'])
#    env.Append(LIBS = ['m'])
#    env.Append(LIBS = ['pthread'])
#    env.Append(LIBS = ['c'])
#    if (env['PLATFORM'] != 'darwin'):
#       env.Append(LIBS = ['crypt'])
#    env.Append(LIBS = ['util'])
#    if (env['PLATFORM'] != 'darwin'):
#       env.Append(LIBS = ['nsl'])
#    env.Append(CCFLAGS = '-DREENTRANT')
#    env.Append(CCFLAGS = '-D_GNU_SOURCE')
#    env.Append(CCFLAGS = '-DDEBIAN')
#    env.Append(CCFLAGS = '-fstack-protector')
#    env.Append(CCFLAGS = '-Wno-literal-suffix')
#    env.Append(CCFLAGS = '-fno-strict-aliasing')
#    env.Append(CCFLAGS = '-pipe')
#    env.Append(CCFLAGS = '-I'+pthread_include)
#    env.Append(CCFLAGS = '-DLARGEFILE_SOURCE')
#    env.Append(CCFLAGS = '-D_FILE_OFFSET_BITS=64')

#    # Interface
#    if not env.GetOption('clean'):
#      if (env['PLATFORM'] == 'darwin'):
#         os.system("make -f Makefile.darwin.interface perl")
#      else:
#         os.system("make -f Makefile.interface perl")
#    else:
#      if (env['PLATFORM'] == 'darwin'):
#         os.system("make -f Makefile.darwin.interface perlclean")
#      else:
#         os.system("make -f Makefile.interface perlclean")
# ###################################################################
# # Finally, compile!

# #####################################################
# # Assemble plugin path
# ####################################################
# #pluginpath = ['plugins2/']
# pluginpath = ['plugins/']
# miamipluginpath = getEnvVar('PLUMA_PLUGIN_PATH', '')
# if (miamipluginpath != ''):
#    pluginpaths = miamipluginpath.split(':') #+ ":" + pluginpath
#    pluginpath += pluginpaths
# #print "PLUGIN DIRECTORIES: ", pluginpath
# ######################################################
# # C++ Plugins
# toExport = ['envPlugin']
# if (docuda == 1):
#    toExport.append('envPluginCUDA')
# for folder in pluginpath:
#  env.Append(CCFLAGS = '-I'+folder)
#  if (docuda==1):
#     envPluginCUDA.Append(NVCCFLAGS = '-I'+folder)
#  #if (os.path.isfile(folder+'/SConscript')):
#  #   SConscript(folder+'/SConscript')
#  #else:
#  #   print "NOT A FILE: ", folder+'/SConscript'
#  sconscripts = Glob(folder+'/*/SConscript')
#  #pluginlist_cpp = Glob('plugins/*/*.cpp')
#  pluginlist_cpp = Glob(folder+'/*/*.cpp')
#  if (len(pluginlist_cpp) != 0 and len(sconscripts) != 0):
#    #print "READY TO RUN"
#    for sconscript in sconscripts:
#      SConscript(sconscript, exports=toExport)
#    #print "DONE"
#  cur_folder = ''
#  firsttime = True
#  for plugin in pluginlist_cpp:
#    print(plugin)
#    #www = input()
#    if (plugin.get_dir() != cur_folder):  # New context
#     if (not firsttime):
#       #if (env['PLATFORM'] == 'darwin'):
#       #  sourcefiles.append("PluginManager.cpp")
#       if (len(pluginName) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#       else:
#          x = envPlugin.SharedLibrary(pluginName, sourcefiles)
#     cur_folder = plugin.get_dir()
#     pluginName = ''
#     sourcefiles = []
#     if (env['PLATFORM'] == 'darwin'):
#         sourcefiles.append("PluginManager.cpp")
#    firsttime = False
#    filename = plugin.get_path()
#    if (filename.endswith('Plugin.cpp')):
#       pluginName = filename[0:filename.find('.cpp')]
#    sourcefiles.append(filename)
#  if (len(pluginName) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#  else:
#          x = envPlugin.SharedLibrary(pluginName, sourcefiles)
#    #print plugin
#    #print x
# ######################################################
# ######################################################
# # CUDA Plugins
# if (docuda==1):
#  for folder in pluginpath:
#    #sconscripts = Glob(folder+'/*/SConscript')
#    #for sconscript in sconscripts:
#    #  SConscript(sconscript, exports = 'envPluginCUDA')
#    pluginlist_cu = Glob(folder+'/*/*.cu')
#    #if (len(pluginlist_cu) != 0 and len(sconscripts) != 0):
#      #print "READY TO RUN AGAIN"
#      #print pluginlist_cu[0], sconscripts[0]
#      #for sconscript in sconscripts:
#      #  SConscript(sconscript, exports='envPluginCUDA')
#      #print "DONE"
#    cur_folder = ''
#    firsttime = True
#    for plugin in pluginlist_cu:
#      if (plugin.get_dir() != cur_folder):  # New context
#       if (not firsttime):
#         if (len(sharedpluginname) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#         else:
#          x = envPluginCUDA.Command(sharedpluginname+".so", sourcefiles, "nvcc -o $TARGET -shared "+srcstring+"-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I"+os.environ['PWD'])
#       cur_folder = plugin.get_dir()
#       sharedpluginname = ''
#       sourcefiles = []
#       srcstring = ''
#      firsttime = False
#      filename = plugin.get_path()
#      if (filename.endswith('Plugin.cu')):
#        name = filename.replace(str(plugin.get_dir()), '')
#        sharedpluginname = str(plugin.get_dir())+"/lib"+name[1:name.find('.cu')]
#      sourcefiles.append(filename)
#      srcstring += filename+' '
#    if (not firsttime and len(sharedpluginname) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#    elif (not firsttime):
#          x = envPluginCUDA.Command(sharedpluginname+".so", sourcefiles, "nvcc -o $TARGET -shared "+srcstring+"-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I"+os.environ['PWD'])


#       #envPluginCUDA.Object(source=plugin)
#       #pluginname = plugin.path[0:len(plugin.path)-3]
#       #pluginfile = plugin.name[0:len(plugin.name)-3]
#       #sharedpluginname = pluginname.replace(pluginfile, 'lib'+pluginfile)
#       #envPluginCUDA.Command(sharedpluginname+".so", pluginname+".o", "nvcc -o $TARGET -shared $SOURCE")
# #####################################################

#  cur_folder = ''
#  firsttime = True
#  for plugin in pluginlist_cpp:
#    if (plugin.get_dir() != cur_folder):  # New context
#     if (not firsttime):
#       if (len(pluginName) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#       else:
#          #print("HERE NOW")
#          #hhh = input()
#          x = envPlugin.SharedLibrary(pluginName, sourcefiles)
#     cur_folder = plugin.get_dir()
#     pluginName = ''
#     sourcefiles = []
#    firsttime = False
#    filename = plugin.get_path()
#    if (filename.endswith('Plugin.cpp')):
#       pluginName = filename[0:filename.find('.cpp')]
#    sourcefiles.append(filename)
#  if (len(pluginName) == 0):
#          print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
#  else:
#          print(sourcefiles)
#          ggg = input()
#          x = envPlugin.SharedLibrary(pluginName, sourcefiles)






# # Main Executable
# folder_sets = [['.', 'languages'], ['PluGen']]
# sources = [[], []]
# targets = ['pluma', 'PluGen/plugen']

# for i in range(0,len(targets)):
#    for folder in folder_sets[i]:
#       env.Append(CCFLAGS = '-I'+folder)
#       sources[i].append(Glob(folder+'/*.cpp'))
#    #print "ST: ", sources[i], targets[i]
#    env.Program(source=sources[i], target=targets[i])


#for folder in folders:
#   env.Append(CCFLAGS = '-I'+folder)
#   sources.append(Glob(folder+'/*.cpp'))
#env.Program(source=sources, target='miami')
#env.Append(CCFLAGS = '-I.')
#env.Append(CCFLAGS = '-Ilanguages')

#env.Program(source=['miami.cpp', Glob('languages/*.cpp')], target='miami')

###################################################################


