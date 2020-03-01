#!python
# Copyright (C) 2017-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor configuration.
#
# TODO: Test portability with Windows systems using MSVC.
# TODO: Merge variable assignments.
from copy import copy, deepcopy
import logging
import os
import subprocess
import sys
import warnings

# The current Sconstruct does not support windows variants.
if sys.platform.startswith('windows'):
    logging.error('Windows is not currently supported for compilation')
    Exit(1)

###################################################################
# HELPER FUNCTIONS
def SourcePath(p):
    return os.path.relpath('./src/'+p)
###################################################################

###################################################################
# Add Commndline Options to our Scons build
AddOption('--prefix',
    dest = 'prefix',
    type = 'string',
    nargs = 1,
    default = '/usr/local',
    help = '$PREFIX folder for installed libraries and headers')

# Language options.
# We assume True for common plugins UNLESS the enduser specifies
# False. For CUDA, we assume False since CUDA availability is not expected.
# on most end-user systems.
#
# Note we have 'pluma.cxx' and scons, so we appropriately assume the enduser
# has C++ and Python installed.
AddOption('--with-cuda',
    dest = 'with-cuda',
    action = 'store_true',
    default = False,
    help = 'Enable CUDA plugin compilation')

AddOption('--without-python',
    dest = 'without-python',
    action = 'store_true',
    default = False,
    help = 'Disable Python plugin compilation')

AddOption('--without-r',
    dest = 'without-r',
    action = 'store_true',
    default = False,
    help = 'Disable R plugin compilation')

AddOption('--without-perl',
    dest = 'without-perl',
    action = 'store_true',
    default = False,
    help = 'Disable Perl plugin compilation')

if 'clean' in sys.argv:
    SetOption('clean', True)
###################################################################

###################################################################

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.

env = Environment(
    ENV = os.environ,
    CC = os.getenv('CC', 'cc'),
    CXX = os.getenv('CXX', 'c++'),
    CCFLAGS = '-pipe -fPIC -g -O2 -fstack-protector -Wpedantic',
    CFLAGS = os.getenv('CFLAGS', '-std=c11'),
    CXXFLAGS = os.getenv('CXXFLAGS', '-std=c++11'),
    LDFLAGS = os.getenv('LDFLAGS', '-D_FORTIFY_SOURCE=2')
)

env.Clean('python', [
    SourcePath('PyPluMA.o'),
    SourcePath('PyPluMA_wrap.o'),
    SourcePath('_PyPluMA.so')
])

if not sys.platform.startswith('darwin'):
    env.Append(LINKFLAGS = ['-rdynamic'])

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

###################################################################
# Check for headers, libraries, and build sub-environments
# for plugins.
###################################################################
config = Configure(env)

if not config.CheckCC():
    logging.error('!! C compiler could not output a valid executable')
    Exit(1)

if not config.CheckCXX():
    logging.error('!! CXX compiler could not output a valid executable')
    Exit(1)

if not config.CheckLib('c'):
    logging.error('!! Could not detec a valid C library')
    Exit(1)

if not config.CheckLib('pthread'):
    logging.error('!! Could not find `pthread` library')
    Exit(1)
else:
    config.env.Append(LIBS = ['pthread'])

if not config.CheckHeader('math.h') or not config.CheckLib('m'):
    logging.error('!! Math library not found')
    Exit(1)
else:
    config.env.Append(
        CCFLAGS = ['-DHAS_MATH'],
        LIBS = ['m']
    )

if not config.CheckLib('dl'):
    logging.error('!! Could not find dynamic linking library `dl`')
    Exit(1)
else:
    config.env.Append(LIBS = ['dl'])

if not config.CheckLib('util'):
    logging.error('!! Could not find `util` library')
    Exit(1)
else:
    config.env.Append(LIBS = ['util'])

config.env.Append(
    CCFLAGS = ['-I'+SourcePath('languages'), '-I'+SourcePath('PluGen')],
    CXXFLAGS = ['-I'+SourcePath('languages'), '-I'+SourcePath('PluGen')]
)

env = config.Finish()

envPlugin = copy(env)

envPlugin.Append(SHCCFLAGS = ['-std=c11', '-fpermissive', '-I'+SourcePath('')],
    SHCXXFLAGS = ['std=c++11''-fpermissive', '-I'+SourcePath('')])

envPluginPython = envPlugin.Clone()
envPluginPerl = envPlugin.Clone()
envPluginR = envPlugin.Clone()

# Not checking for existance of `python` since this is a python script.
if not GetOption('without-python'):
    config = Configure(envPluginPython)

    python_includes = subprocess.check_output(
        ['python-config', '--includes'],
        encoding = 'utf8'
    ).split(' ')
    python_libs = subprocess.check_output(
        ['python-config', '--libs'],
        encoding = 'utf8'
    ).split(' ')

    python_version = sys.version[0:3]

    print(python_libs)

    config.env.Append(CXXFLAGS = [python_includes])
    config.env.Append(LIBS = [python_libs])

    if python_version[0] == 2:
        warnings.warn('Python2 has reached EOL. Please update to python3.',
        DeprecationWarning)

    config.env.Append(CXXFLAGS = ['-DHAVE_PYTHON'])

    if (python_version[0] == '3' and not sys.platform.startswith('darwin')):
        config.env.AppendUnique(LIBS = ['rt'])

    envPluginPython = config.Finish()

# Checking for existance of `perl`.
if not GetOption('without-perl'):
    if not env.WhereIs('perl'):
        logging.error('!! Perl executables not found')
        Exit(1)

    config = Configure(envPluginPerl)
    config.env.Append(PERL_INCLUDE_DIR = os.getenv('PERL_INCLUDE_DIR', ''),
        PERL_LIB_DIR = os.getenv('PERL_LIB_DIR', ''))

    perl_include = subprocess.check_output(['perl', '-e "print qq(@INC)"']).decode('ascii').split(' ')

    for include_dir in perl_include:
        print(include_dir)
        config.env.AppendUnique(CXXFLAGS = ['-I'+include_dir])

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
    if not env.WhereIs('R') and not env.WhereIs('Rscript'):
        logging.error('!! R executables not found')
        Exit(1)

    config = Configure(envPluginR)

    r_lib = subprocess.check_output(['pkg-config', '--libs-only-L', 'libR']).decode('ascii')
    config.env.ParseConfig('pkg-config --cflags-only-I --libs-only-L --libs-only-l libR')
    rinside_lib = os.getenv('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
    rinside_include = os.getenv('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
    rcpp_include = os.getenv('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')

    config.env.Append(
        CXXFLAGS = ['-I'+rinside_include, '-I'+rcpp_include, '-DNDEBUG',
            '-fpermissive', '--param=ssp-buffer-size=4', '-Wformat',
            '-Wformat-security', '-Werror=format-security', '-DHAVE_R'
        ],
        LIBPATH = [rinside_lib],
        rpath = ','+rinside_lib,
        LIBS = ['R', 'RInside']
    )
###################################################################
# Execute compilation for our plugins.
# Note: CUDA is already prepared from the initial environment setup.
###################################################################

###################################################################
# PYTHON PLUGINS
if not GetOption('without-python'):
    envPluginPython.StaticObject(source = SourcePath('PluMA.cxx'), target = 'PyPluMA.o')
    envPluginPython.StaticObject(source = SourcePath('PyPluMA_wrap.cxx'), target = 'PyPluMA_wrap.o')
###################################################################

###################################################################
# PERL PLUGINS
# if not GetOption('without-perl'):
#     envPluginPerl.SharedObject(source = 'PluMA.cxx', target = 'PerlPluMA.o')
#     envPluginPerl.SharedObject(source = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.o')
#     envPluginPerl.SharedLibrary(srouce = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.so')
###################################################################
# R PLUGINS
if not GetOption('without-r'):
    envPluginR.SharedObject(source = SourcePath('PluMA.cxx'), target = 'RPluMA.o')
    envPluginR.SharedObject(source = SourcePath('RPluMA_wrap.cxx'), target = 'RPluMA_wrap.o')
    envPluginR.Append(CCFLAGS = ['-Wl,-Bsymbolic-functions', '-Wl,-z,relro'])
    envPluginR.SharedLibrary('RPluMA_wrap.cxx')
###################################################################

###################################################################
# Finally, compile!
# It is dangerous to go alone, take this: 𐃉
###################################################################

###################################################################
# Assemble plugin path
###################################################################
pluginPath = Glob(SourcePath('') + 'plugins/**')

###################################################################

###################################################################
# C++ Plugins
toExport = ['envPlugin', 'envPluginPython', 'envPluginPerl', 'envPluginR']
if GetOption('with-cuda'):
    toExport.append('envPluginCUDA')
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
# folder_sets = [[os.path.relpath('languages')], [os.path.relpath('PluGen')]]
# sources = [[], []]
# targets = ['pluma', 'PluGen/plugen']

# languages = Glob(SourcePath('languages/*.cxx'))
# plugen = Glob(SourcePath('PluGen/*.cxx'))
#
# env.Program(target = 'PluGen/plugen', source = plugen)
# env.Program(target = 'pluma', source = languages)

# for i in range(0,len(targets)):
#     for folder in folder_sets[i]:
#         env.Append(CCFLAGS = ['-I'+folder])
#         sources[i].append(Glob(folder+'/*.{cpp,cxx}'))
#     env.Program(target = targets[i], source = sources[i])
###################################################################
