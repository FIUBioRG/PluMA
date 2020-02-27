#!python
# Copyright (C) 2017-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor configuration.
#
# TODO: Test portability with Windows systems using MSVC.
# TODO: Merge variable assignments.
import os
import sys
import subprocess
import logging
import warnings
from copy import copy

# The current Sconstruct does not support windows variants.
if sys.platform.startswith('windows'):
    logging.error('Windows is not currently supported for compilation')
    Exit(1)

###################################################################
# HELPER FUNCTIONS

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
# Clean stage. Completes previous compilation cleaning and exits.
if GetOption('clean'):
    if (sys.platform.startswith('darwin')):
        os.system('make -f Makefile.darwin.interface clean')
    else:
        os.system('make -f Makefile.interface clean')
    Exit(0)
###################################################################

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.

env = Environment(
    ENV = os.environ,
    CC = os.getenv('CC', 'cc'),
    CXX = os.getenv('CXX', 'c++'),
    CCFLAGS = os.getenv('CFLAGS', '-mtune=generic -pipe -fPIC -g'
    + '-02 -fstack-protector -pedantic'),
    CXXFLAGS = os.getenv('CXXFLAGS', '-mtune=generic -pipe -fPIC -g'
    + '-O2 -fstack-protector -pedantic'),
    LDFLAGS = os.getenv('LDFLAGS', '-D_FORTIFY_SOURCE=2')
)

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
    config.env.Append(CCFLAGS = ['-DHAS_MATH'], LIBS = ['m'])

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

env = config.Finish()

envPlugin = copy(env)

envPlugin.Append(SHCCFLAGS = ['-fpermissive', '-I'+os.getcwd()],
    SHCXXFLAGS = ['std=c++11''-fpermissive', '-I'+os.getcwd()])

envPluginPython = copy(envPlugin)
envPluginPerl = copy(envPluginPython)
envPluginR = copy(envPluginPython)

# Not checking for existance of `python` since this is a python script.
if not GetOption('without-python'):
    config = Configure(envPluginPython)

    python_prefix = subprocess.check_output(['python-config', '--prefix']).decode('ascii')
    python_version = sys.version[0:3]
    python_include = python_prefix+'/include/'+python_version
    python_lib = python_prefix+'/lib/'+python_version+'/config'

    if python_version[0] == 2:
        warnings.warn('Python2 has reached EOL. Please update to python3.',
        DeprecationWarning)

    config.env.Append(CXXFLAGS = ['-I'+python_include, '-DHAVE_PYTHON'],
        LIBPATH = [python_lib],
        LIBS = [python_version])

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
        config.env.Append(CCFLAGS = ['-I'+include_dir],
            CXXFLAGS = ['-I'+include_dir])

    config.env.Append(
        CCFLAGS = ['-DHAVE_PERL', '-Wl,-E',
            '-fstack-protector', '-DREENTRANT', '-Wno-literal-suffix',
            '-fno-strict-aliasing', '-DLARGE_SOURCE', '-D_FILE_OFFSET_BITS=64'],
        LIBS = ['perl'])

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
# # if not GetOption('without-python'):
#     if (sys.platform.startswith('darwin')):
#         os.system('make -f Makefile.darwin.interface python')
#     else:
#         os.system('make -f Makefile.interface python')
envPlugin.Object(source = 'PluMA.cxx', target = 'PyPluMA.o')
envPlugin.Object(source = 'PyPluMA_wrap.cxx', target = 'PyPluMA_wrap.o')
###################################################################

###################################################################
# PERL PLUGINS
if not GetOption('without-perl'):
    # if (env['PLATFORM'] == 'darwin'):
    #     os.system('make -f Makefile.darwin.interface perl')
    # else:
    #     os.system('make -f Makefile.interface perl')
    envPluginPerl.Object(source = 'PluMA.cxx', target = 'PerlPluMA.o')
    envPluginPerl.Object(source = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.o')
    envPluginPerl.SharedLibrary(srouce = 'PerlPluMA_wrap.cxx', target = 'PerlPluMA_wrap.so')
###################################################################
# R PLUGINS
if not GetOption('without-r'):
    # if (sys.platform.startswith('darwin')):
    #     os.system('make -f Makefile.darwin.interface r')
    # else:
    #     os.system('make -f Makefile.interface r')
    envPluginR.Object(source = 'PluMA.cxx', target = 'RPluMA.o')
    envPluginR.Object(source = 'RPluMA_wrap.cxx', target = 'RPluMA_wrap.o')
    envPluginR.Append(CXXFLAGS = ['-Wl,-Bsymbolic-functions', '-Wl,-z,relro'])
    envPluginR.SharedLibrary(source = 'RPluMA_wrap.cxx', target = 'RPluma_wrap.so')
###################################################################

###################################################################
# Finally, compile!
# It is dangerous to go alone, take this: 𐃉
###################################################################

###################################################################
# Assemble plugin path
###################################################################
pluginPath = Glob('plugins/**')

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

languages = Glob('./languages/**/*.{cpp,cxx}')
env.Append(CXXFLAGS = ['-I'+os.path.relapth('languages')])
env.Program(target = 'pluma', source = languages)

plugen = Glob('./PluGen/**/*.{cpp,cxx}')
env.Append(CXXFLAGS = ['-I'+os.path.relpath('PluGen')])
env.Program(target = 'PluGen/plugen', source = plugen)

# for i in range(0,len(targets)):
#     for folder in folder_sets[i]:
#         env.Append(CCFLAGS = ['-I'+folder])
#         sources[i].append(Glob(folder+'/*.{cpp,cxx}'))
#     env.Program(target = targets[i], source = sources[i])
###################################################################
