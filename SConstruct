import os
import sys
import subprocess
#######################################################################
# HELPER FUNCTION
def getEnvVar(name, default):
   if (os.environ.__contains__(name)):
      return os.environ[name]
   else:
      return default
#######################################################################

# Gets the environment variables set by the user
env = Environment(ENV = os.environ)
envPlugin = Environment(ENV = os.environ)
env.Append(CC = os.getenv("CC", "gcc"))
env.Append(CXX = os.getenv("CXX", "g++"))
env.Append(CFLAGS = os.getenv("CFLAGS", ""))
env.Append(CXXFLAGS = os.getenv("CXXFLAGS", "-std=c++11 -fPIC -g -O2"))
env.Append(LDFLAGS = os.getenv("LDFLAGS", ""))
env.Append(CCFLAGS = '-std=c++0x')
env.Append(LIBS = ['m', 'dl'])
if (env['PLATFORM'] != 'darwin'):
   env.Append(LINKFLAGS = '-rdynamic')

envPlugin.Append(SHCCFLAGS = '-fpermissive')
envPlugin.Append(SHCCFLAGS = '-I'+os.environ['PWD'])
envPlugin.Append(SHCCFLAGS = '-std=c++0x')
#envPlugin.Append(LIBS = ['gsl'])

# Language arguments
# For now I am assuming 1 UNLESS they specify 0
# Note we have 'pluma.cpp' and scons, so I must assume they have C++
# and Python
docuda = ARGUMENTS.get('cuda', 1)
#print docuda
r = ARGUMENTS.get('r', 1)
perl = ARGUMENTS.get('perl', 1)
python = ARGUMENTS.get('python', 1)

env.Append(CC = )

###################################################################
# USED TO CHECK FOR LIBRARIES
###################################################################
conf = Configure(env)

###################################################################
# PYTHON INFORMATION
python_dir = sys.exec_prefix
python_version = sys.version[0:3]
python_include = '-I'+getEnvVar('PYTHON_INCLUDE_DIR', python_dir+'/include/python'+python_version)
python_lib = getEnvVar('PYTHON_LIB_DIR', python_dir+'/lib/python'+python_version+'/config')
if (python == 1):
   env.Append(CCFLAGS = python_include)
   env.Append(LIBPATH = [python_lib])
   env.Append(LIBS = ['python'+python_version])
   env.Append(CCFLAGS = '-DHAVE_PYTHON')
   env.Append(LIBS = ['pthread'])
   env.Append(LIBS = ['util'])
   if (python_version[0] == '3' and env['PLATFORM'] != 'darwin'):
      env.Append(LIBS = ['rt'])
   # Interface
   if not env.GetOption('clean'):
      if (env['PLATFORM'] == 'darwin'):
         os.system("make -f Makefile.darwin.interface PYTHON_VERSION="+python_version+" PYTHON_LIB="+python_lib+" PYTHON_INCLUDE="+python_include+" python")
      else:
         os.system("make -f Makefile.interface PYTHON_INCLUDE="+python_include+" python")
   else:
     if (env['PLATFORM'] == 'darwin'):
        os.system("make -f Makefile.darwin.interface pythonclean")
     else:
        os.system("make -f Makefile.interface pythonclean")
###################################################################


###################################################################
# R (IF INSTALLED)
if (r==1):
   # Get the installation directory of R
   r_lib = getEnvVar('R_LIB_DIR', '/usr/local/lib/R/')
   r_include = getEnvVar('R_INCLUDE_DIR', '/usr/share/R/include/')
   rinside_lib = getEnvVar('RINSIDE_LIB_DIR', r_lib+'/site-library/RInside/lib')
   rinside_include = getEnvVar('RINSIDE_INCLUDE_DIR', r_lib+'/site-library/RInside/include')
   rcpp_include = getEnvVar('RCPP_INCLUDE_DIR', r_lib+'/site-library/Rcpp/include')

   env.Append(CCFLAGS = '-I'+r_include)
   env.Append(CCFLAGS = '-I'+rinside_include)
   env.Append(CCFLAGS = '-I'+rcpp_include)
   env.Append(LIBPATH = [r_lib])
   env.Append(LIBPATH = [rinside_lib])
   env.Append(rpath = ','+rinside_lib)
   env.Append(CCFLAGS = '-DHAVE_R')
   env.Append(LIBS = ['R'])
   env.Append(LIBS = ['RInside'])
   if not env.GetOption('clean'):
     if (env['PLATFORM'] == 'darwin'):
        os.system("make -f Makefile.darwin.interface R_INCLUDE=-I"+r_include+" R_LIB=-L"+r_lib+" r")
     else:
        os.system("make -f Makefile.interface R_INCLUDE=-I"+r_include+" R_LIB=-L"+r_lib+" rpath=,"+rinside_lib+" r")
   else:
     if (env['PLATFORM'] == 'darwin'):
        os.system("make -f Makefile.darwin.interface rclean")
     else:
        os.system("make -f Makefile.interface rclean")
###################################################################

###################################################################
if (docuda==1):
   envPluginCUDA = Environment(ENV = os.environ)
   envPluginCUDA.Tool('cuda')
   envPluginCUDA2 = Environment(ENV = os.environ)
   envPluginCUDA.Tool('cuda')
   envPluginCUDA.Append(NVCCFLAGS = '-I'+os.environ['PWD'])
   envPluginCUDA.Append(NVCCFLAGS = ['-arch=sm_30'])
   envPluginCUDA.Append(NVCCFLAGS = ['--ptxas-options=-v'])
   envPluginCUDA.Append(NVCCFLAGS = ['-std=c++11'])
   envPluginCUDA.Append(NVCCFLAGS = ['-Xcompiler'])
   envPluginCUDA.Append(NVCCFLAGS = ['-fpic'])
   envPluginCUDA2['NVCCFLAGS'] = ['-shared']
###################################################################

###################################################################
if (perl==1):
   # Get the installation directory of Perl
   # Note: Will assume /usr/ if PERLHOME is unset
   # Also need pthreads, assuming /usr/local is PTHREADHOME is unset
   #perlhome = getEnvVar('PERLHOME', '/usr/')
   pthread_lib = getEnvVar('PTHREAD_LIB_DIR', '/usr/local/lib')
   pthread_include = getEnvVar('PTHREAD_LIB_DIR', '/usr/local/include')
   getversion = str(subprocess.check_output(['perl', '-V'])).split(' ')
   perlversion = getversion[5]+'.'+getversion[7]
   perlarch = getEnvVar('PERLARCH', '')
   if (env['PLATFORM'] != 'darwin'):
      perl_include = getEnvVar('PERL_INCLUDE_DIR', '/usr/lib/perl/'+perlversion+'/CORE')
   else:
      perl_include = getEnvVar('PERL_INCLUDE_DIR', '/usr/perl'+getversion[5]+'/'+perlversion+'.0/'+perlarch+'/CORE')
   perl_lib = getEnvVar('PERL_LIB_DIR', perl_include)

   env.Append(LIBPATH = [perl_lib])
   env.Append(CCFLAGS = '-I'+perl_include)
   env.Append(CCFLAGS = '-DHAVE_PERL')
   #env.Append(LIBPATH = [perl_lib])
   env.Append(CCFLAGS = '-Wl,-E')
   env.Append(CCFLAGS = '-fstack-protector')
   env.Append(LIBPATH = [pthread_lib])
   env.Append(LIBS = ['perl'])
   env.Append(LIBS = ['dl'])
   env.Append(LIBS = ['m'])
   env.Append(LIBS = ['pthread'])
   env.Append(LIBS = ['c'])
   if (env['PLATFORM'] != 'darwin'):
      env.Append(LIBS = ['crypt'])
   env.Append(LIBS = ['util'])
   if (env['PLATFORM'] != 'darwin'):
      env.Append(LIBS = ['nsl'])
   env.Append(CCFLAGS = '-DREENTRANT')
   env.Append(CCFLAGS = '-D_GNU_SOURCE')
   env.Append(CCFLAGS = '-DDEBIAN')
   env.Append(CCFLAGS = '-fstack-protector')
   env.Append(CCFLAGS = '-Wno-literal-suffix')
   env.Append(CCFLAGS = '-fno-strict-aliasing')
   env.Append(CCFLAGS = '-pipe')
   env.Append(CCFLAGS = '-I'+pthread_include)
   env.Append(CCFLAGS = '-DLARGEFILE_SOURCE')
   env.Append(CCFLAGS = '-D_FILE_OFFSET_BITS=64')

   # Interface
   if not env.GetOption('clean'):
     if (env['PLATFORM'] == 'darwin'):
        os.system("make -f Makefile.darwin.interface perl")
     else:
        os.system("make -f Makefile.interface perl")
   else:
     if (env['PLATFORM'] == 'darwin'):
        os.system("make -f Makefile.darwin.interface perlclean")
     else:
        os.system("make -f Makefile.interface perlclean")
###################################################################
# Finally, compile!

#####################################################
# Assemble plugin path
####################################################
#pluginpath = ['plugins2/']
pluginpath = ['plugins/']
miamipluginpath = getEnvVar('PLUMA_PLUGIN_PATH', '')
if (miamipluginpath != ''):
   pluginpaths = miamipluginpath.split(':') #+ ":" + pluginpath
   pluginpath += pluginpaths
#print "PLUGIN DIRECTORIES: ", pluginpath
######################################################
# C++ Plugins
toExport = ['envPlugin']
if (docuda == 1):
   toExport.append('envPluginCUDA')
for folder in pluginpath:
 env.Append(CCFLAGS = '-I'+folder)
 if (docuda==1):
    envPluginCUDA.Append(NVCCFLAGS = '-I'+folder)
 #if (os.path.isfile(folder+'/SConscript')):
 #   SConscript(folder+'/SConscript')
 #else:
 #   print "NOT A FILE: ", folder+'/SConscript'
 sconscripts = Glob(folder+'/*/SConscript')
 #pluginlist_cpp = Glob('plugins/*/*.cpp')
 pluginlist_cpp = Glob(folder+'/*/*.cpp')
 if (len(pluginlist_cpp) != 0 and len(sconscripts) != 0):
   #print "READY TO RUN"
   for sconscript in sconscripts:
     SConscript(sconscript, exports=toExport)
   #print "DONE"
 cur_folder = ''
 firsttime = True
 for plugin in pluginlist_cpp:
   if (plugin.get_dir() != cur_folder):  # New context
    if (not firsttime):
      if (len(pluginName) == 0):
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
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
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
 else:
         x = envPlugin.SharedLibrary(pluginName, sourcefiles)
   #print plugin
   #print x
######################################################
######################################################
# CUDA Plugins
if (docuda==1):
 for folder in pluginpath:
   #sconscripts = Glob(folder+'/*/SConscript')
   #for sconscript in sconscripts:
   #  SConscript(sconscript, exports = 'envPluginCUDA')
   pluginlist_cu = Glob(folder+'/*/*.cu')
   #if (len(pluginlist_cu) != 0 and len(sconscripts) != 0):
     #print "READY TO RUN AGAIN"
     #print pluginlist_cu[0], sconscripts[0]
     #for sconscript in sconscripts:
     #  SConscript(sconscript, exports='envPluginCUDA')
     #print "DONE"
   cur_folder = ''
   firsttime = True
   for plugin in pluginlist_cu:
     if (plugin.get_dir() != cur_folder):  # New context
      if (not firsttime):
        if (len(sharedpluginname) == 0):
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
        else:
         x = envPluginCUDA.Command(sharedpluginname+".so", sourcefiles, "nvcc -o $TARGET -shared "+srcstring+"-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I"+os.environ['PWD'])
      cur_folder = plugin.get_dir()
      sharedpluginname = ''
      sourcefiles = []
      srcstring = ''
     firsttime = False
     filename = plugin.get_path()
     if (filename.endswith('Plugin.cu')):
       name = filename.replace(str(plugin.get_dir()), '')
       sharedpluginname = str(plugin.get_dir())+"/lib"+name[1:name.find('.cu')]
     sourcefiles.append(filename)
     srcstring += filename+' '
   if (len(sharedpluginname) == 0):
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
   else:
         x = envPluginCUDA.Command(sharedpluginname+".so", sourcefiles, "nvcc -o $TARGET -shared "+srcstring+"-std=c++11 -arch=sm_30 --ptxas-options=-v -Xcompiler -fpic -I"+os.environ['PWD'])


      #envPluginCUDA.Object(source=plugin)
      #pluginname = plugin.path[0:len(plugin.path)-3]
      #pluginfile = plugin.name[0:len(plugin.name)-3]
      #sharedpluginname = pluginname.replace(pluginfile, 'lib'+pluginfile)
      #envPluginCUDA.Command(sharedpluginname+".so", pluginname+".o", "nvcc -o $TARGET -shared $SOURCE")
#####################################################

 cur_folder = ''
 firsttime = True
 for plugin in pluginlist_cpp:
   if (plugin.get_dir() != cur_folder):  # New context
    if (not firsttime):
      if (len(pluginName) == 0):
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
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
         print("WARNING: NULL PLUGIN IN FOLDER: "+folder+", IGNORING")
 else:
         x = envPlugin.SharedLibrary(pluginName, sourcefiles)

# Main Executable
folder_sets = [['.', 'languages'], ['PluGen']]
sources = [[], []]
targets = ['pluma', 'PluGen/plugen']

for i in range(0,len(targets)):
   for folder in folder_sets[i]:
      env.Append(CCFLAGS = '-I'+folder)
      sources[i].append(Glob(folder+'/*.cpp'))
   #print "ST: ", sources[i], targets[i]
   env.Program(source=sources[i], target=targets[i])

#for folder in folders:
#   env.Append(CCFLAGS = '-I'+folder)
#   sources.append(Glob(folder+'/*.cpp'))
#env.Program(source=sources, target='miami')
#env.Append(CCFLAGS = '-I.')
#env.Append(CCFLAGS = '-Ilanguages')

#env.Program(source=['miami.cpp', Glob('languages/*.cpp')], target='miami')

###################################################################
