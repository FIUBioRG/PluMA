# Copyright (C) 2017-2020 FIUBioRG
# SPDX-License-Identifier: MIT
#
# Application constructor configuration.
#
# TODO: Test portability with Windows systems using MSVC.
# TODO: Merge variable assignments.
# TODO: Ensure indentation matches throughout file, 2-space or 4.
# TODO: Recheck for snake-case and camel-case usage.
import os
import sys
import subprocess
import logging
import warnings

if sys.platform.startswith('windows'):
    logging.error('Windows is not currently supported for compilation')
    Exit(1)

###################################################################
# HELPER FUNCTIONS
#
# TODO: is this needed alongside `os.getenv`?
# def getEnvVar(name, default):
#     if (os.environ.__contains__(name)):
#         return os.environ[name]
#     else:
#         return default
###################################################################

###################################################################
# Add Commndline Options to our Scons build
AddOption('--prefix',
    dest='prefix',
    type='string',
    nargs=1,
    default='/usr/local',
    help='$PREFIX folder for installed libraries and headers')

# Language arguments
# For now, we assume 1 (true) for common plugins UNLESS the enduser specifies
# 0 (false). For CUDA, we assume 0.
# Note we have 'pluma.cpp' and scons, so I must assume the enduser has C++
# and Python.

# cuda = ARGUMENTS.get('cuda', 1)
# r = ARGUMENTS.get('r', 1)
# perl = ARGUMENTS.get('perl', 1)
# python = ARGUMENTS.get('python', 1)
AddOption('--with-cuda',
    dest='with-cuda',
    action='store_true',
    default=False,
    help='Enable CUDA plugin compilation')
AddOption('--without-python',
    dest='without-python',
    action='store_true',
    default=False
    help='Disable Python plugin compilation')
AddOption('--without-r',
    dest='without-r',
    action='store_true',
    default=False,
    help='Disable R plugin compilation')
AddOption('--without-perl',
    dest='without-perl',
    action='store_true',
    default=False,
    help='Disable Perl plugin compilation')

###################################################################
# Gets the environment variables set by the user on the OS level or
# defaults to 'sane' values.
env = Environment(ENV = os.environ,
    CC = os.getenv('CC', 'cc'),
    CXX = os.getenv('CXX', 'c++'),
    CCFLAGS = os.getenv('CFLAGS', '-std=c11 -mtune=generic -pipe -fPIC -g -02 -fstack-protector -pedantic'),
    CXXFLAGS = os.getenv('CXXFLAGS', '-std=c++11 -mtune=generic -pipe -fPIC -g -O2 -fstack-protector -pedantic'),
    LDFLAGS = os.getenv('LDFLAGS', '-D_FORTIFY_SOURCE=2'),
    PERL_INCLUDE_DIR = os.getenv('PERL_INCLUDE_DIR', ''),
    PERL_LIB_DIR = os.getenv('PERL_LIB_DIR', ''))

envPlugin = Environment(ENV = os.environ)

if not sys.platform.startswith('darwin'):
   env.AppendUnique(LINKFLAGS = ['-rdynamic'])

# Append shared plugin flags.
envPlugin.Append(SHCCFLAGS = ['-fpermissive'])
envPlugin.Append(SHCCFLAGS = ['-I'+os.getcwd())
envPlugin.Append(SHCCFLAGS = ['-std=c++11'])

###################################################################
# USED TO CHECK FOR LIBRARIES
###################################################################
if not env.GetOption('clean'):
    conf = Configure(env)

    if not conf.CheckCXX():
        logging.error('!! Could not detect a valid C++ compiler')
        Exit(1)

    if not conf.CheckLib('c'):
        logging.error('!! Could not detec a valid C library')
        Exit(1)

    if not conf.CheckLib('pthread'):
        logging.error('!! Could not find `pthread` library')
        Exit(1)
    else:
        config.env.AppendUnique(LIBS = ['pthread'])

    if not conf.CheckHeader('math.h') or not conf.CheckLib('m'):
        logging.error('!! Math library not found')
        Exit(1)
    else:
        config.env.AppendUnique(CFLAGS = ['-DHAS_MATH'],
          LIBS = ['m'])

    if not conf.CheckLib('dl'):
        logging.error('!! Could not find dynamic linking library `dl`')
        Exit(1)
    else:
        config.env.AppendUnique(LIBS = ['dl'])

    if not conf.CheckLib('util'):
        logging.error('!! Could not find `util` library')
        Exit(1)
    else:
        config.env.AppendUnique(LIBS = ['util'])

    if not GetOption('wihout-python'):
        python_prefix = str(subprocess.check_output('python-config', '--prefix'))
        python_version = sys.version[0:3]
        python_include = python_prefix+'/include/'+python_version
        python_lib = python_prefix+'/lib/'+python_version+'/config'

        if python_version[0] == 2:
            warnings.warn('Python2 has reached EOL. Please update to python3.',
            DeprecationWarning)

        config.env.AppendUnique(CCFLAGS = ['-I'+python_include,
            '-DHAVE_PYTHON'],
        LIBPATH = [python_lib],
        LIBS = [python_version])

        if (python_version[0] == '3' and not sys.platform.startswith('darwin')):
            config.env.AppendUnique(LIBS = ['rt'])

    if not GetOption('without-r') and not WhereIs('R')
    and not WhereIs('Rscript'):
        logging.error('!! R executable not found')
        Exit(1)
    else:
        r_lib = os.system('pkg-config --libs-only-L')
        env.ParseConfig('pkg-config --cflags-only-I --libs-only-L --libs-only-l libR')
        # r_lib = os.getenv('R_LIB_DIR', '/usr/local/lib/R/')
        # r_include = os.getenv('R_INCLUDE_DIR', '/usr/share/R/include/')
        rinside_lib = os.getenv('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
        rinside_include = os.getenv('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
        rcpp_include = os.getenv('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')

        # env.AppendUnique(CCFLAGS = ['-I'+r_include])
        env.AppendUnique(CCFLAGS = ['-I'+rinside_include])
        env.AppendUnique(CCFLAGS = ['-I'+rcpp_include])
        # env.AppendUnique(LIBPATH = [r_lib])
        env.AppendUnique(LIBPATH = [rinside_lib])
        env.AppendUnique(rpath = ','+rinside_lib)
        env.AppendUnique(CCFLAGS = ['-DHAVE_R'])
        env.AppendUnique(LIBS = ['R'])
        env.AppendUnique(LIBS = ['RInside'])

    if not GetOption('perl'):
        # getversion = str(subprocess.check_output(['perl', '-V'])).split(' ')
        # perlversion = getversion[5]+'.'+getversion[7]
        # perlarch = os.getenv('PERLARCH', '')
        perl_include = str(subprocess.check_output(['perl', '-e', '"print qq(@INC)"']).split(' ')
        # if sys.platform.startswith('darwin'):
        #     perl_include = os.getenv('PERL_INCLUDE_DIR', '/usr/perl'+getversion[5]+'/'+perlversion+'.0/'+perlarch+'/CORE')
        # else:
        #     perl_include = os.getenv('PERL_INCLUDE_DIR', '/usr/lib/perl/'+perlversion+'/CORE')

        # perl_lib = os.getenv('PERL_LIB_DIR', perl_include)

        for include_dir in perl_include:
            env.AppendUnique(CCFLAGS = ['-I'+include_dir])


        env.Append(CCFLAGS = ['-DHAVE_PERL'])
        env.AppendUnique(CCFLAGS = ['-Wl,-E', '-fstack-protector',
        '-DDEBIAN -D_GNU_SOURCE -DREENTRANT', '-Wno-literal-suffix',
        '-fno-strict-aliasing', '-DLARGE_SOURCE', '-D_FILE_OFFSET_BITS=64'])
        # env.Append(CCFLAGS = ['-DREENTRANT'])
        # env.Append(CCFLAGS = ['-D_GNU_SOURCE'])
        # env.Append(CCFLAGS = ['-DDEBIAN'])
        # env.AppendUnique(CCFLAGS = ['-Wno-literal-suffix'])
        # env.AppendUnique(CCFLAGS = ['-fno-strict-aliasing'])
        # env.AppendUnique(CCFLAGS = ['-pipe'])
        # env.Append(CCFLAGS = ['-I'+pthread_include])
        # env.Append(CCFLAGS = ['-DLARGEFILE_SOURCE'])
        # # env.Append(CCFLAGS = ['-D_FILE_OFFSET_BITS=64'])
        # env.Append(LIBPATH = [pthread_lib])
        env.AppendUnique(LIBPATH = [perl_lib])
        env.Append(LIBS = ['perl'])
        # env.AppendUnique(LIBS = ['dl'])
        # env.AppendUnique(LIBS = ['m'])
        # env.AppendUnique(LIBS = ['pthread'])
        # env.AppendUnique(LIBS = ['c'])
        # env.AppendUnique(LIBS = ['util'])
        if (env['PLATFORM'] != 'darwin'):
            env.Append(LIBS = ['crypt'])
            env.Append(LIBS = ['nsl'])


    if GetOption('cuda'):
        envPluginCUDA = Environment(ENV = os.environ,
            CUDA_PATH = os.getenv('CUDA_PATH', '/usr/local/cuda'),
            NVCCFLAGS = os.getenv('NVCCFLAGS', ''))
        envPluginCUDA.Tool('cuda')
        envPluginCUDA.Append(NVCCFLAGS = ['-I'+os.environ['PWD']])
        envPluginCUDA.Append(NVCCFLAGS = ['-arch=sm_30'])
        envPluginCUDA.Append(NVCCFLAGS = ['--ptxas-options=-v'])
        envPluginCUDA.Append(NVCCFLAGS = ['-std=c++11'])
        envPluginCUDA.Append(NVCCFLAGS = ['-Xcompiler'])
        envPluginCUDA.Append(NVCCFLAGS = ['-fPIC'])
        envPluginCUDA2 = Environment(ENV = os.environ,
            CUDA_PATH = os.getenv('CUDA_PATH', '/usr/local/cuda'))
        envPluginCUDA2.Tool('cuda')
        envPluginCUDA2.Append(NVCCFLAGS = ['-shared'])

    env = config.Finish()

###################################################################
# PYTHON INFORMATION
#
# TODO: Change variables to use python executable to determine paths.
# python_dir = sys.exec_prefix
# python_version = sys.version[0:3]
# python_include = '-I'+os.getenv('PYTHON_INCLUDE_DIR', python_dir+'/include/python'+python_version)
# python_lib = os.getenv('PYTHON_LIB_DIR', python_dir+'/lib/python'+python_version+'/config')
if not GetOption('without-python'):
    # env.Append(CCFLAGS = [python_include])
    # envPlugin.Append(CCFLAGS = ['-DHAVE_PYTHON'])
    # env.Append(LIBPATH = [python_lib])
    # env.Append(LIBS = ['python'+python_version])
    # env.Append(CCFLAGS = ['-DHAVE_PYTHON'])
    # env.Append(LIBS = ['pthread'])
    # env.Append(LIBS = ['util'])
    # if (python_version[0] == '3' and env['PLATFORM'] != 'darwin'):
    #     env.Append(LIBS = ['rt'])
    # Interface
    if not env.GetOption('clean'):
        if (sys.platform.startswith('darwin')):
            os.system('make -f Makefile.darwin.interface PYTHON_VERSION='+python_version+' PYTHON_LIB='+python_lib+' PYTHON_INCLUDE='+python_include+' python')
        else:
            os.system('make -f Makefile.interface PYTHON_INCLUDE='+python_include+' python')
    else:
        if (sys.platform.startswith('darwin')):
            os.system('make -f Makefile.darwin.interface pythonclean')
        else:
            os.system('make -f Makefile.interface pythonclean')
###################################################################


###################################################################
# R (IF INSTALLED) INFORMATION
#
# TODO: Use R binary to determine variable assignements.
# TODO: Shift variables into appending/merging with already defined variables.
if not GetOption('without-r'):
    # Get the installation directory of R
    # r_lib = os.getenv('R_LIB_DIR', '/usr/local/lib/R/')
    # r_include = os.getenv('R_INCLUDE_DIR', '/usr/share/R/include/')
    # rinside_lib = os.getenv('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
    # rinside_include = os.getenv('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
    # rcpp_include = os.getenv('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')
    #
    # env.Append(CCFLAGS = ['-I'+r_include])
    # env.Append(CCFLAGS = ['-I'+rinside_include])
    # env.Append(CCFLAGS = ['-I'+rcpp_include])
    # env.Append(LIBPATH = [r_lib])
    # env.Append(LIBPATH = [rinside_lib])
    # env.Append(rpath = ','+rinside_lib)
    # env.Append(CCFLAGS = ['-DHAVE_R'])
    # env.Append(LIBS = ['R'])
    # env.Append(LIBS = ['RInside'])
    if not env.GetOption('clean'):
        if (sys.platform.startswith('darwin')):
            os.system('make -f Makefile.darwin.interface R_INCLUDE=-I'+r_include+' R_LIB=-L'+r_lib+' r')
        else:
            os.system('make -f Makefile.interface R_INCLUDE=-I'+r_include+' R_LIB=-L'+r_lib+' rpath=,'+rinside_lib+' r')
    else:
        if (sys.platform.startswith('darwin')):
            os.system('make -f Makefile.darwin.interface rclean')
        else:
            os.system('make -f Makefile.interface rclean')
###################################################################

###################################################################
# CUDA INFORMATION
#
# TODO: Shift variables into appending/merging with already defined variables.
if GetOption('with-cuda'):
    envPluginCUDA = Environment(ENV = os.environ,
        CUDA_PATH = os.getenv('CUDA_PATH', '/usr/local/cuda'),
        NVCCFLAGS = os.getenv('NVCCFLAGS', ''))
    envPluginCUDA.Tool('cuda')
    envPluginCUDA.Append(NVCCFLAGS = ['-I'+os.environ['PWD']])
    envPluginCUDA.Append(NVCCFLAGS = ['-arch=sm_30'])
    envPluginCUDA.Append(NVCCFLAGS = ['--ptxas-options=-v'])
    envPluginCUDA.Append(NVCCFLAGS = ['-std=c++11'])
    envPluginCUDA.Append(NVCCFLAGS = ['-Xcompiler'])
    envPluginCUDA.Append(NVCCFLAGS = ['-fPIC'])
    envPluginCUDA2 = Environment(ENV = os.environ,
        CUDA_PATH = os.getenv('CUDA_PATH', '/usr/local/cuda'))
    envPluginCUDA2.Tool('cuda')
    envPluginCUDA2.Append(NVCCFLAGS = ['-shared'])
###################################################################

###################################################################
# PERL INFORMATION
#
# TODO: Use R binary to determine variable assignements.
# TODO: Shift variables into appending/merging with already defined variables.
if GetOption('with-perl'):
    # Get the installation directory of Perl
    # Note: Will assume /usr/ if PERLHOME is unset
    # Also need pthreads, assuming /usr/local is PTHREADHOME is unset
    # perlhome = os.getenv('PERLHOME', '/usr/')
    # pthread_lib = os.getenv('PTHREAD_LIB_DIR', '/usr/local/lib')
    # pthread_include = os.getenv('PTHREAD_LIB_DIR', '/usr/local/include')
    # getversion = str(subprocess.check_output(['perl', '-V'])).split(' ')
    # perlversion = getversion[5]+'.'+getversion[7]
    # perlarch = os.getenv('PERLARCH', '')
    # if (env['PLATFORM'] != 'darwin'):
    #     perl_include = os.getenv('PERL_INCLUDE_DIR', '/usr/lib/perl/'+perlversion+'/CORE')
    # else:
    #     perl_include = os.getenv('PERL_INCLUDE_DIR', '/usr/perl'+getversion[5]+'/'+perlversion+'.0/'+perlarch+'/CORE')
    # perl_lib = os.getenv('PERL_LIB_DIR', perl_include)
    #
    #
    # env.Append(CCFLAGS = ['-I'+perl_include])
    # env.Append(CCFLAGS = ['-DHAVE_PERL'])
    # env.AppendUnique(CCFLAGS = ['-Wl,-E'])
    # env.AppendUnique(CCFLAGS = ['-fstack-protector'])
    # env.Append(CCFLAGS = ['-DREENTRANT'])
    # env.Append(CCFLAGS = ['-D_GNU_SOURCE'])
    # env.Append(CCFLAGS = ['-DDEBIAN'])
    # env.AppendUnique(CCFLAGS = ['-Wno-literal-suffix'])
    # env.AppendUnique(CCFLAGS = ['-fno-strict-aliasing'])
    # env.AppendUnique(CCFLAGS = ['-pipe'])
    # env.Append(CCFLAGS = ['-I'+pthread_include])
    # env.Append(CCFLAGS = ['-DLARGEFILE_SOURCE'])
    # env.Append(CCFLAGS = ['-D_FILE_OFFSET_BITS=64'])
    # env.Append(LIBPATH = [pthread_lib])
    # env.Append(LIBPATH = [perl_lib])
    # env.Append(LIBS = ['perl'])
    # env.AppendUnique(LIBS = ['dl'])
    # env.AppendUnique(LIBS = ['m'])
    # env.AppendUnique(LIBS = ['pthread'])
    # env.AppendUnique(LIBS = ['c'])
    # env.AppendUnique(LIBS = ['util'])
    # if (env['PLATFORM'] != 'darwin'):
    #     env.Append(LIBS = ['crypt'])
    #     env.Append(LIBS = ['nsl'])

    # Interface
    if not env.GetOption('clean'):
        if (env['PLATFORM'] == 'darwin'):
           os.system('make -f Makefile.darwin.interface perl')
        else:
            os.system('make -f Makefile.interface perl')
    else:
        if (env['PLATFORM'] == 'darwin'):
            os.system('make -f Makefile.darwin.interface perlclean')
        else:
            os.system('make -f Makefile.interface perlclean')

###################################################################
# Finally, compile!
# It is dangerous to go alone, take this: 𐃉
###################################################################

###################################################################
# Assemble plugin path
###################################################################
pluginpath = ['plugins/']
miamipluginpath = os.getenv('PLUMA_PLUGIN_PATH', '')
if (miamipluginpath != ''):
    pluginpaths = miamipluginpath.split(':') #+ ':' + pluginpath
    pluginpath += pluginpaths
#print 'PLUGIN DIRECTORIES: ', pluginpath
###################################################################
# C++ Plugins
toExport = ['envPlugin']
if GetOption('with-cuda'):
    toExport.append('envPluginCUDA')
for folder in pluginpath:
    env.AppendUnique(CCFLAGS = ['-I'+folder])
    if GetOption('with-cuda'):
        envPluginCUDA.AppendUnique(NVCCFLAGS = ['-I'+folder])
    #if (os.path.isfile(folder+'/SConscript')):
    #   SConscript(folder+'/SConscript')
    #else:
    #   print('NOT A FILE: %s' % folder+'/SConscript')
    sconscripts = Glob(folder+'/*/SConscript')
    #pluginlist_cpp = Glob('plugins/*/*.cpp')
    pluginlist_cpp = Glob(folder+'/*/*.cpp')
    if (len(pluginlist_cpp) != 0 and len(sconscripts) != 0):
        #print('READY TO RUN')
        for sconscript in sconscripts:
            SConscript(sconscript, exports=toExport)
        #print('DONE')
    cur_folder = ''
    firsttime = True
    for plugin in pluginlist_cpp:
        if (plugin.get_dir() != cur_folder):  # New context
            if (not firsttime):
                if (len(pluginName) == 0):
                    print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                else:
                    x = envPlugin.SharedLibrary(pluginName, sourcefiles)
            cur_folder = plugin.get_dir()
            pluginName = ''
            sourcefiles = []
        firsttime = False
        filename = plugin.get_path()
        if (filename.endswith('Plugin.cpp')):
            pluginName = filename[0:filename.find('.cpp')]
        sourcefiles.append(filename)
    if (len(pluginName) == 0):
         print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
    else:
         x = envPlugin.SharedLibrary(pluginName, sourcefiles)
###################################################################

###################################################################
# CUDA Plugins
#
# TODO: Compress if-else statements?
if (cuda==1):
    for folder in pluginpath:
        # sconscripts = Glob(folder+'/*/SConscript')
        # for sconscript in sconscripts:
        #     SConscript(sconscript, exports = 'envPluginCUDA')
        pluginlist_cu = Glob(folder+'/*/*.cu')
        # if (len(pluginlist_cu) != 0 and len(sconscripts) != 0):
        #     print 'READY TO RUN AGAIN'
        #     print pluginlist_cu[0], sconscripts[0]
        # for sconscript in sconscripts:
        #     SConscript(sconscript, exports='envPluginCUDA')
        #     print 'DONE'
        cur_folder = ''
        firsttime = True
        for plugin in pluginlist_cu:
            if (plugin.get_dir() != cur_folder):  # New context
                if (not firsttime):
                    if (len(sharedpluginname) == 0):
                        print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                    else:
                        x = envPluginCUDA.Command(sharedpluginname+'.so', sourcefiles, 'nvcc -o $TARGET -shared '+srcstring+'-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I'+os.environ['PWD'])
            cur_folder = plugin.get_dir()
            sharedpluginname = ''
            sourcefiles = []
            srcstring = ''
        firsttime = False
        filename = plugin.get_path()
        if (filename.endswith('Plugin.cu')):
            name = filename.replace(str(plugin.get_dir()), '')
            sharedpluginname = str(plugin.get_dir())+'/lib'+name[1:name.find('.cu')]
        sourcefiles.append(filename)
        srcstring += filename+' '
    if (len(sharedpluginname) == 0):
        print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
    else:
        x = envPluginCUDA.Command(sharedpluginname+'.so', sourcefiles, 'nvcc -o $TARGET -shared '+srcstring+'-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I'+os.environ['PWD'])

    #envPluginCUDA.Object(source=plugin)
    #pluginname = plugin.path[0:len(plugin.path)-3]
    #pluginfile = plugin.name[0:len(plugin.name)-3]
    #sharedpluginname = pluginname.replace(pluginfile, 'lib'+pluginfile)
    #envPluginCUDA.Command(sharedpluginname+'.so', pluginname+'.o', 'nvcc -o $TARGET -shared $SOURCE')

    # Repeat of CPP plugins?
    cur_folder = ''
    firsttime = True
    for plugin in pluginlist_cpp:
        if (plugin.get_dir() != cur_folder):  # New context
            if (not firsttime):
                if (len(pluginName) == 0):
                    print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING' % folder)
                else:
                    x = envPlugin.SharedLibrary(pluginName, sourcefiles)
            cur_folder = plugin.get_dir()
            pluginName = ''
            sourcefiles = []
        firsttime = False
        filename = plugin.get_path()
        if (filename.endswith('Plugin.cpp')):
            pluginName = filename[0:filename.find('.cpp')]
        sourcefiles.append(filename)
    if (len(pluginName) == 0):
        print('WARNING: NULL PLUGIN IN FOLDER: %s, IGNORING', folder)
    else:
        x = envPlugin.SharedLibrary(pluginName, sourcefiles)
###################################################################
# Main Executable
folder_sets = [['.', 'languages'], ['PluGen']]
sources = [[], []]
targets = ['pluma', 'PluGen/plugen']

for i in range(0,len(targets)):
    for folder in folder_sets[i]:
        env.AppendUnique(CCFLAGS = ['-I'+folder])
        sources[i].append(Glob(folder+'/*.cpp'))
    env.Program(source=sources[i], target=targets[i])

# for folder in folders:
#     env.Append(CCFLAGS = '-I'+folder)
#     sources.append(Glob(folder+'/*.cpp'))
# env.Program(source=sources, target='miami')
# env.Append(CCFLAGS = '-I.')
# env.Append(CCFLAGS = '-Ilanguages')

# env.Program(source=['miami.cpp', Glob('languages/*.cpp')], target='miami')
###################################################################
