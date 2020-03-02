#!python
# Copyright (C) 2017-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor configuration.
#
# TODO: Test portability with Windows systems using MSVC.
# TODO: Merge variable assignments.
import logging
import os
from os.path import relpath
import subprocess
import sys
import warnings

from build_config import *
from build_support import *

# The current Sconstruct does not support windows variants.
if sys.platform.startswith('windows'):
    logging.error('Windows is not currently supported for compilation')
    Exit(1)

###################################################################
# Add Commndline Options to our Scons build
#
# We assume with-* options to be False for common plugins UNLESS the enduser
# specifies True. For CUDA, we assume False since CUDA availability is not
# expected on most end-user systems.

AddOption(
    '--prefix',
    dest = 'prefix',
    default = '/usr/local',
    help = 'PREFIX for local installations'
)

AddOption(
    '--with-cuda',
    dest = 'with-cuda',
    action = 'store_true',
    default = False,
    help = 'Enable CUDA plugin compilation'
)

AddOption(
    '--without-python',
    dest = 'without-python',
    action = 'store_true',
    default = False,
    help = 'Disable Python plugin compilation'
)

AddOption(
    '--without-perl',
    dest = 'without-perl',
    action = 'store_true',
    default = False,
    help = 'Disable Perl plugin compilation'
)

AddOption(
    '--without-r',
    dest = 'without-r',
    action = 'store_true',
    default = False,
    help = 'Disable R plugin compilation'
)

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.
###################################################################
env = Environment(
    ENV = os.environ,
    CC = os.getenv('CC', 'cc'),
    CFLAGS = os.getenv('CFLAGS', '-std=c11'),
    CXX = os.getenv('CXX', 'c++'),
    CXXFLAGS = os.getenv('CXXFLAGS', '-std=c++11'),
    LDFLAGS = os.getenv('LDFLAGS', '-D_FORTIFY_SOURCE=2'),
    SHCFLAGS = '-fpermissive -Isrc/',
    SHCXXFLAGS = '-fpermissive -Isrc/'
)
###################################################################

###################################################################
# Either clean folders from previous Scons runs or begin assembling
# `PluMA` and its associated plugins
if env.GetOption('clean'):
    env.Clean('python', [
        Glob('./**/__pycache__'),
        ObjectPath('PyPluMA.o'),
        ObjectPath('PyPluMA_wrap.o'),
        OutPath('_PyPluMA.so')
    ])

    env.Clean('scons', [
        relpath('.sconf_temp'),
        relpath('.sconsign.dblite'),
        relpath('config.log'),
        OutPath('.'),
        ObjectPath('.')
    ])

    env.Clean('all', [ 'python' ])
else:

    envPluginPython = None
    envPluginPerl = None
    envPluginR = None
    envPluginCuda = None

    if not sys.platform.startswith('darwin'):
        env.Append(LINKFLAGS = ['-rdynamic'])

    ###################################################################
    # Check for headers, libraries, and build sub-environments
    # for plugins.
    ###################################################################
        config = Configure(env)

        if env.get('debug', 0):
            env.Append(
                CPPDEFINES = ['DEBUG', '_DEBUG'],
                CXXFLAGS = ['-g']
            )
        else:
            env.Append(
                CPPDEFINES = ['NDEBUG'],
                CXXFLAGS = ['-O2']
            )

        config.env.AppendUnique(
            CPPPATH = include_search_path,
            CXXFLAGS = ['-pipe', '-fPIC', '-fstack-protector'],
            LIBPATH = lib_search_path
        )


        if not config.CheckCXX():
            logging.error('!! CXX compiler could not output a valid executable')
            Exit(1)

        if not config.CheckSHCXX():
            logging.error('!! Shared CXX compiler could not output a valid executable')
            Exit(1)

        if not config.CheckLibWithHeader('m', 'math.h', 'c'):
            logging.error('!! Math library not found')
            Exit(1)

        if not config.CheckLib('pthread'):
            logging.error('!! Could not find `pthread` library')
            Exit(1)

        if GetOption('without-python') and not config.CheckProg('python') or not config.CheckProg('python-config'):
            logging.error('!! Could not find a valid `python` installation')
            Exit(1)

        if GetOption('without-perl') and not config.CheckProg('perl'):
            logging.error('!! Could not find a valid `perl` installation`')
            Exit(1)
        else:
            perl_include = cmdline('perl -e "print qq(@INC)"').split(' ')

            for include_dir in perl_include:
                if "core_perl" in include_dir:
                    include_dir = include_dir + "/CORE"
                config.env.AppendUnique(CPPPATH = [include_dir])

            if not config.CheckHeader('EXTERN.h'):
                logging.error('!! Could not find `EXTERN.h`')
                Exit(1)

            config.env.AppendUnique(
                CCFLAGS = [
                    '-DHAVE_PERL',
                    '-Wl,-E',
                    '-DREENTRANT',
                    '-Wno-literal-suffix',
                    '-fno-strict-aliasing'
                ],
                LDFLAGS = [
                    '-DLARGE_SOURCE'
                    '-D_FILE_OFFSET_BITS=64'
                ],
                LIBS = ['perl']
            )

        if GetOption('without-r') and not config.CheckProg('R') and not env.WhereIs('Rscript'):
            logging.error('!! Could not find a valid `R` installation`')
            Exit(1)

        env = config.Finish()

        envPluginPython = env.Clone()
        envPluginPerl = env.Clone()
        envPluginR = env.Clone()

        # Not checking for existance of `python` since this is a python script.
        if not GetOption('without-python'):

            config = Configure(envPluginPython)

            # python_includes = subprocess.check_output(
            #     ['python-config', '--includes'],
            #     encoding = 'utf8'
            # ).split(' ')
            #
            # python_libs = subprocess.check_output(
            #     ['python-config', '--libs'],
            #     encoding = 'utf8'
            # ).split(' ')
            env.ParseConfig("python-config --includes --libs")

            python_version = sys.version[0:3]
            #
            # config.env.Append(CXXFLAGS = [python_includes])
            # config.env.Append(LIBS = [python_libs])

            if python_version[0] == 2:
                warnings.warn('Python2 has reached EOL. Please update to python3.',
                DeprecationWarning)

            config.env.Append(CXXFLAGS = ['-DHAVE_PYTHON'])

            if (python_version[0] == '3' and not sys.platform.startswith('darwin')):
                config.env.AppendUnique(LIBS = ['rt'])

            envPluginPython = config.Finish()

        # Checking for existance of `perl`.
        if not GetOption('without-perl'):

            config = Configure(envPluginPerl)

            config.env.Append(PERL_INCLUDE_DIR = os.getenv('PERL_INCLUDE_DIR', ''),
                PERL_LIB_DIR = os.getenv('PERL_LIB_DIR', ''))

            config.env.Append(
                CCFLAGS = ['-DHAVE_PERL', '-Wl,-E',
                    '-fstack-protector', '-DREENTRANT', '-Wno-literal-suffix',
                    '-fno-strict-aliasing', '-DLARGE_SOURCE', '-D_FILE_OFFSET_BITS=64'],
                LIBS = ['perl']
            )

            if sys.platform.startswith('darwin'):
                config.env.Append(LIBS = ['crypt', 'nsl'])

            envPluginPerl = config.Finish()

        # Checking for existance of `R` and `Rscript`.
        if not GetOption('without-r'):

            config = Configure(envPluginR)

            r_lib = subprocess.check_output(['pkg-config', '--libs-only-L', 'libR']).decode('ascii')
            config.env.ParseConfig('pkg-config --cflags-only-I --libs-only-L --libs-only-l libR')
            rinside_lib = os.getenv('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
            rinside_include = os.getenv('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
            rcpp_include = os.getenv('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')

            config.env.Append(CXXFLAGS = ['-Wl,-Bsymbolic-functions', '-Wl,-z,relro'])

            config.env.AppendUnique(
                CXXFLAGS = [
                    '-fpermissive',
                    '--param=ssp-buffer-size=4',
                    '-Wformat',
                    '-Wformat-security',
                    '-Werror=format-security',
                    '-DHAVE_R'
                ],
                CPPPATH = ['-I'+rinside_include, '-I'+rcpp_include],
                LIBPATH = [rinside_lib],
                rpath = ','+rinside_lib,
                LIBS = ['R', 'RInside']
            )

            envPluginR = config.Finish()

        envPluginCuda = Environment(
            ENV = os.environ,
            CUDA_PATH = [os.getenv('CUDA_PATH', '/usr/local/cuda')],
            NVCCFLAGS = [
                os.getenv('NVCCFLAGS', [
                    '-I'+os.getcwd(),
                    '-arch=sm_30',
                    '--ptxas-options=-v',
                    '-std=c++11',
                    '-Xcompiler',
                    '-fPIC'
                    '-I'+os.getcwd()
                ])
            ]
        )
        envPluginCuda.Tool('cuda')

        # Export `envPlugin` and `envPluginCUDA`
        Export('env')
        if not GetOption('without-python'):
            Export('envPluginPython')
        if not GetOption('without-perl'):
            Export('envPluginPerl')
        if not GetOption('without-r'):
            Export('envPluginR')
        if GetOption('with-cuda'):
            Export('envPluginCUDA')

    ###################################################################
    # Execute compilation for our plugins.
    # Note: CUDA is already prepared from the initial environment setup.
    ###################################################################

    ###################################################################
    # PYTHON PLUGINS
    if not GetOption('without-python'):
        envPluginPython.SharedObject(source = SourcePath('PluMA.cxx'), target = ObjectPath('PyPluMA.o'))
        envPluginPython.SharedObject(source = SourcePath('PyPluMA_wrap.cxx'), target = ObjectPath('PyPluMA_wrap.o'))
        envPluginPython.SharedLibrary(target = OutPath('_PyPluMA_wrap.so'), source = [ ObjectPath('PyPluMA.o'), ObjectPath('PyPluMA_wrap.o') ])
    ###################################################################

    ###################################################################
    # PERL PLUGINS
# if not GetOption('without-perl'):
#     envPluginPerl.SharedObject(source = 'PluMA.cxx', target = 'PerlPluMA.o')
#     envPluginPerl.SharedObject(source = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.o')
#     envPluginPerl.SharedLibrary(srouce = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.so')
    ###################################################################
    # R PLUGINS
# if not GetOption('without-r'):
#     envPluginR.SharedObject(source = 'src/PluMA.cxx', target = 'obj/RPluMA.o')
#     envPluginR.SharedObject(source = 'src/RPluMA_wrap.cxx', target = 'obj/RPluMA_wrap.o')
#     envPluginR.SharedLibrary(source = 'src/RPluMA_wrap.cxx', target = 'obj/RPluMA_wrap.so')
    ###################################################################

    ###################################################################
    # Finally, compile!
    # It is dangerous to go alone, take this: 𐃉
    ###################################################################

    ###################################################################
    # Assemble plugin path
    pluginPath = Glob(SourcePath('') + 'plugins/**')
    ###################################################################

    ##################################################################
    # C++ Plugins

    for folder in pluginPath:
        envPlugin.AppendUnique(CCFLAGS = ['-I'+folder])
        if GetOption('with-cuda'):
            envPluginCUDA.AppendUnique(NVCCFLAGS = ['-I'+folder])
        sconscripts = Glob(folder+'/*/SConscript')
        pluginListCXX = Glob(folder+'/*/*.{cpp, cxx}')
        if (len(pluginListCXX) != 0 and len(sconscripts) != 0):
            for sconscript in sconscripts:
                SConscript(sconscript, exports = toExport)
        curFolder = ''
        firstTime = True
        for plugin in pluginListCXX:
            if (plugin.get_dir() != curFolder):  # New context
                if (not firstTime):
                    if (len(pluginName) == 0):
                        print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                    else:
                        envPlugin.SharedLibrary(pluginName, sourceFiles)
                curFolder = plugin.get_dir()
                pluginName = ''
                sourceFiles = []
            firstTime = False
            filename = plugin.get_path()
            if (filename.endswith('Plugin.cpp')):
                pluginName = filename[0:filename.find('.cpp')]
            sourceFiles.append(filename)
        if (len(pluginName) == 0):
             print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
        else:
             envPlugin.SharedLibrary(pluginName, sourceFiles)
    ###################################################################

    ###################################################################
    # CUDA Plugins
    #
    # TODO: Compress if-else statements?
    if GetOption('with-cuda'):
        for folder in pluginpath:
            pluginsCUDA = Glob(folder+'/*/*.cu')
            curFolder = ''
            firstTime = True
            for plugin in pluginsCUDA:
                if (plugin.get_dir() != curFolder):  # New context
                    if (not firstTime):
                        if (len(sharedPluginName) == 0):
                            logging.warning('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                        else:
                            envPluginCUDA.SharedLibrary(sharedPluginName, sourceFiles)
                curFolder = plugin.get_dir()
                sharedPluginName = ''
                sourceFiles = []
                srcStr = ''
            firstTime = False
            filename = plugin.get_path()
            if (filename.endswith('Plugin.cu')):
                name = filename.replace(str(plugin.get_dir()), '')
                sharedPluginName = str(plugin.get_dir())+'/lib'+name[1:name.find('.cu')]
            sourceFiles.append(filename)
            srcStr += filename+' '
        if (len(sharedPluginName) == 0):
            logging.warning('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
        else:
            envPluginCUDA.Command(sharedPluginName, sourceFiles)

        # Repeat of C++ plugins?
        # Consider DRY-ing?
        curFolder = ''
        firstTime= True
        for plugin in pluginListCXX:
            if (plugin.get_dir() != curFolder):  # New context
                if (not firstTime):
                    if (len(pluginName) == 0):
                        logging.warning('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                    else:
                        envPlugin.SharedLibrary(pluginName, sourcefiles)
                curFolder = plugin.get_dir()
                pluginName = ''
                sourceFiles = []
            firstTime = False
            filename = plugin.get_path()
            if (filename.endswith('Plugin.cpp')):
                pluginName = filename[0:filename.find('.cpp')]
            sourceFiles.append(filename)
        if (len(pluginName) == 0):
            logging.warn('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING', folder)
        else:
            envPlugin.SharedLibrary(pluginName, sourceFiles)
    ###################################################################
    # Main Executable & PluGen
    # folder_sets = [[os.path.relpath('languages')], [os.path.relpath(
    # sources = [[], []]
    # targets = ['pluma', 'PluGen/plugen']

    env.AppendUnique(
        LDFLAGS = ['-I./src/', '-I./src/languages', '-I./src/PluGen']
    )

    env.Program('out/PluGen/plugen', Glob(SourcePath('PluGen/*.cxx')))
    env.Program('out/pluma', [ Glob(SourcePath('languages/*.cxx')), SourcePath('PluginManager.h')])

    # for i in range(0,len(targets)):
    #   for folder in folder_sets[i]:
    #       env.Append(CCFLAGS = ['-I'+folder])
    #           sources[i].append(Glob(folder+'/*.{cpp,cxx}'))
    #   env.Program(target = targets[i], source = sources[i])
###################################################################
