#
# SConstruct - build script for the SDL port of fceux
#
# You can adjust the BoolVariables below to include/exclude features at compile-time
#
# You may need to wipe the scons cache ("scons -c") and recompile for some options to take effect.
#
# Use "scons" to compile and "scons install" to install.
#

FCEUX_DIR = 'fceux'

import os
import sys
import platform 

opts = Variables()
opts.AddVariables( 
  BoolVariable('FRAMESKIP', 'Enable frameskipping', 1),
  BoolVariable('OPENGL',    'Enable OpenGL support', 1),
  BoolVariable('LSB_FIRST', 'Least signficant byte first (non-PPC)', 1),
  BoolVariable('DEBUG',     'Build with debugging symbols', 1),
  BoolVariable('LUA',       'Enable Lua support', 1),
  BoolVariable('SYSTEM_LUA','Use system lua instead of static lua provided with fceux', 1),
  BoolVariable('SYSTEM_MINIZIP', 'Use system minizip instead of static minizip provided with fceux', 0),
  BoolVariable('NEWPPU',    'Enable new PPU core', 1),
  BoolVariable('CREATE_AVI', 'Enable avi creation support (SDL only)', 1),
  BoolVariable('LOGO', 'Enable a logoscreen when creating avis (SDL only)', 1),
  BoolVariable('GTK', 'Enable GTK2 GUI (SDL only)', 1),
  BoolVariable('GTK3', 'Enable GTK3 GUI (SDL only)', 0),
  BoolVariable('CLANG', 'Compile with llvm-clang instead of gcc', 0)
)
AddOption('--prefix', dest='prefix', type='string', nargs=1, action='store', metavar='DIR', help='installation prefix')

prefix = GetOption('prefix')
env = Environment(options = opts)

#### Uncomment this for a public release ###
#env.Append(CPPDEFINES=["PUBLIC_RELEASE"])
env['DEBUG'] = 0
env['LUA'] = 1
env['CLANG'] = 1
############################################

# LSB_FIRST must be off for PPC to compile
if platform.system == "ppc":
  env['LSB_FIRST'] = 0

# Default compiler flags:
env.Append(CCFLAGS = ['-Wall', '-Wno-write-strings', '-Wno-sign-compare', '-I%s/src/lua/src' % FCEUX_DIR])

if os.environ.has_key('PLATFORM'):
  env.Replace(PLATFORM = os.environ['PLATFORM'])
if os.environ.has_key('CC'):
  env.Replace(CC = os.environ['CC'])
if os.environ.has_key('CXX'):
  env.Replace(CXX = os.environ['CXX'])
if os.environ.has_key('WINDRES'):
  env.Replace(WINDRES = os.environ['WINDRES'])
if os.environ.has_key('CFLAGS'):
  env.Append(CCFLAGS = os.environ['CFLAGS'].split())
if os.environ.has_key('CXXFLAGS'):
  env.Append(CXXFLAGS = os.environ['CXXFLAGS'].split())
if os.environ.has_key('CPPFLAGS'):
  env.Append(CPPFLAGS = os.environ['CPPFLAGS'].split())
if os.environ.has_key('LDFLAGS'):
  env.Append(LINKFLAGS = os.environ['LDFLAGS'].split())

print "platform: ", env['PLATFORM']

# compile with clang
if env['CLANG']:
  env.Replace(CC='clang')
  env.Replace(CXX='clang++')
  env.Append(CXXFLAGS = "-Wno-c++11-extensions")

# special flags for cygwin
# we have to do this here so that the function and lib checks will go through mingw
if env['PLATFORM'] == 'cygwin':
  env.Append(CCFLAGS = " -mno-cygwin")
  env.Append(LINKFLAGS = " -mno-cygwin")
  env['LIBS'] = ['wsock32'];

if env['PLATFORM'] == 'win32':
  env.Append(CPPPATH = [".", "drivers/win/", "drivers/common/", "drivers/", "drivers/win/zlib", "drivers/win/directx", "drivers/win/lua/include"])
  env.Append(CPPDEFINES = ["PSS_STYLE=2", "WIN32", "_USE_SHARED_MEMORY_", "NETWORK", "FCEUDEF_DEBUGGER", "NOMINMAX", "NEED_MINGW_HACKS", "_WIN32_IE=0x0600"])
  env.Append(LIBS = ["rpcrt4", "comctl32", "vfw32", "winmm", "ws2_32", "comdlg32", "ole32", "gdi32", "htmlhelp"])
else:
  conf = Configure(env)
  if conf.CheckFunc('asprintf'):
    conf.env.Append(CCFLAGS = "-DHAVE_ASPRINTF")
  if env['SYSTEM_MINIZIP']:
    assert conf.CheckLibWithHeader('minizip', 'minizip/unzip.h', 'C', 'unzOpen;', 1), "please install: libminizip"
    assert conf.CheckLibWithHeader('z', 'zlib.h', 'c', 'inflate;', 1), "please install: zlib"
    env.Append(CPPDEFINES=["_SYSTEM_MINIZIP"])
  else:
    assert conf.CheckLibWithHeader('z', 'zlib.h', 'c', 'inflate;', 1), "please install: zlib"
  if not conf.CheckLib('SDL'):
    print 'Did not find libSDL or SDL.lib, exiting!'
    Exit(1)
  if env['GTK']:
    if not conf.CheckLib('gtk-x11-2.0'):
      print 'Could not find libgtk-2.0, exiting!'
      Exit(1)
    # Add compiler and linker flags from pkg-config
    config_string = 'pkg-config --cflags --libs gtk+-2.0'
    if env['PLATFORM'] == 'darwin':
      config_string = 'PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig/ ' + config_string
    env.ParseConfig(config_string)
    env.Append(CPPDEFINES=["_GTK2"])
    env.Append(CCFLAGS = ["-D_GTK"])
  if env['GTK3']:
    # Add compiler and linker flags from pkg-config
    config_string = 'pkg-config --cflags --libs gtk+-3.0'
    if env['PLATFORM'] == 'darwin':
      config_string = 'PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig/ ' + config_string
    env.ParseConfig(config_string)
    env.Append(CPPDEFINES=["_GTK3"])
    env.Append(CCFLAGS = ["-D_GTK"])

  ### Lua platform defines
  ### Applies to all files even though only lua needs it, but should be ok
  if env['LUA']:
    env.Append(CPPDEFINES=["_S9XLUA_H"])
    if env['PLATFORM'] == 'darwin':
      # Define LUA_USE_MACOSX otherwise we can't bind external libs from lua
      env.Append(CCFLAGS = ["-DLUA_USE_MACOSX"])    
    if env['PLATFORM'] == 'posix':
      # If we're POSIX, we use LUA_USE_LINUX since that combines usual lua posix defines with dlfcn calls for dynamic library loading.
      # Should work on any *nix
      env.Append(CCFLAGS = ["-DLUA_USE_LINUX"])
    lua_available = False
    if conf.CheckLib('lua5.1'):
      env.Append(LINKFLAGS = ["-ldl", "-llua5.1"])
      lua_available = True
    elif conf.CheckLib('lua'):
      env.Append(LINKFLAGS = ["-ldl", "-llua"])
      lua_available = True
    if lua_available == False:
      print 'Could not find liblua, exiting!'
      Exit(1)
  # "--as-needed" no longer available on OSX (probably BSD as well? TODO: test)
  if env['PLATFORM'] != 'darwin':
    env.Append(LINKFLAGS=['-Wl,--as-needed'])
  
  ### Search for gd if we're not in Windows
  if env['PLATFORM'] != 'win32' and env['PLATFORM'] != 'cygwin' and env['CREATE_AVI'] and env['LOGO']:
    gd = conf.CheckLib('gd', autoadd=1)
    if gd == 0:
      env['LOGO'] = 0
      print 'Did not find libgd, you won\'t be able to create a logo screen for your avis.'
   
  if env['OPENGL'] and conf.CheckLibWithHeader('GL', 'GL/gl.h', 'c', autoadd=1):
    conf.env.Append(CCFLAGS = "-DOPENGL")
  conf.env.Append(CPPDEFINES = ['PSS_STYLE=1'])
  # parse SDL cflags/libs
  env.ParseConfig('sdl-config --cflags --libs')
  
  env = conf.Finish()

if sys.byteorder == 'little' or env['PLATFORM'] == 'win32':
  env.Append(CPPDEFINES = ['LSB_FIRST'])

if env['FRAMESKIP']:
  env.Append(CPPDEFINES = ['FRAMESKIP'])

print "base CPPDEFINES:",env['CPPDEFINES']
print "base CCFLAGS:",env['CCFLAGS']

if env['DEBUG']:
  env.Append(CPPDEFINES=["_DEBUG"], CCFLAGS = ['-g'])
else:
  env.Append(CCFLAGS = ['-O2'])

if env['PLATFORM'] != 'win32' and env['PLATFORM'] != 'cygwin' and env['CREATE_AVI']:
  env.Append(CPPDEFINES=["CREATE_AVI"])
else:
  env['CREATE_AVI']=0;

# Install rules
if prefix == None:
  prefix = "/usr/local"

exe_suffix = ''
if env['PLATFORM'] == 'win32':
  exe_suffix = '.exe'

Export('env')

def flatten(files):
    result = []
    for f in files:
        if isinstance(f, str):
            result.append(f)
        else:
            result.extend(f)
    return result

fceux_file_list = SConscript(FCEUX_DIR+'/src/SConscript')
fceux_file_list = flatten(fceux_file_list)
fceux_file_list = [FCEUX_DIR+'/src/'+f for f in fceux_file_list]
fceux = env.Program(FCEUX_DIR+'/fceux'+exe_suffix, fceux_file_list)

env.Program(target=FCEUX_DIR+"/fceux-net-server", source=[FCEUX_DIR+"/fceux-server/server.cpp", FCEUX_DIR+"/fceux-server/md5.cpp", FCEUX_DIR+"/fceux-server/throttle.cpp"])

fceux_src = FCEUX_DIR+'/fceux' + exe_suffix
fceux_dst = 'bin/fceux' + exe_suffix

fceux_net_server_src = FCEUX_DIR+'/fceux-net-server' + exe_suffix
fceux_net_server_dst = 'bin/fceux-net-server' + exe_suffix

auxlib_src = FCEUX_DIR+'/src/auxlib.lua'
auxlib_dst = 'bin/auxlib.lua'
auxlib_inst_dst = prefix + '/share/fceux/auxlib.lua'

fceux_h_src = FCEUX_DIR+'/src/drivers/win/help/fceux.chm'
fceux_h_dst = 'bin/fceux.chm'

env.Command(fceux_h_dst, fceux_h_src, [Copy(fceux_h_dst, fceux_h_src)])
env.Command(fceux_dst, fceux_src, [Copy(fceux_dst, fceux_src)])
env.Command(fceux_net_server_dst, fceux_net_server_src, [Copy(fceux_net_server_dst, fceux_net_server_src)])
env.Command(auxlib_dst, auxlib_src, [Copy(auxlib_dst, auxlib_src)])

man_src = 'documentation/fceux.6'
man_net_src = 'documentation/fceux-net-server.6'
man_dst = prefix + '/share/man/man6/fceux.6'
man_net_dst = prefix + '/share/man/man6/fceux-net-server.6'

share_src = 'output/'
share_dst = prefix + '/share/fceux/'

env.Install(prefix + "/bin/", fceux)
env.Install(prefix + "/bin/", "fceux-net-server")
# TODO:  Where to put auxlib on "scons install?"
env.Alias('install', env.Command(auxlib_inst_dst, auxlib_src, [Copy(auxlib_inst_dst, auxlib_src)]))
env.Alias('install', env.Command(share_dst, share_src, [Copy(share_dst, share_src)]))
env.Alias('install', env.Command(man_dst, man_src, [Copy(man_dst, man_src)]))
env.Alias('install', env.Command(man_net_dst, man_net_src, [Copy(man_net_dst, man_net_src)]))
env.Alias('install', (prefix + "/bin/"))

env.Append(CPPPATH=['cityhash-1.1.0/src'])
cityEnv = env.Clone()
cityEnv.Append(CPPPATH=['cityhash-1.1.0'])

cityhash = cityEnv.Object('cityhash-1.1.0/src/city.cc')

mongoose = env.Object(target='mongoose', source='mongoose.git/mongoose.c')

tastiness_files = ['tastiness-main.cc', 'emulator.cc', 'headless-driver.cc']
tastiness_files += [mongoose]
tastiness_files += [cityhash]
tastiness_files += fceux_file_list
tastiness_files.remove('fceux/src/drivers/sdl/sdl.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/input.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/config.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/sdl-joystick.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/sdl-sound.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/sdl-throttle.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/sdl-video.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/unix-netplay.cpp')
tastiness_files.remove('fceux/src/drivers/sdl/sdl-opengl.cpp')

env.Append(CPPDEFINES=["USE_WEBSOCKET"])
env.Program(target='bin/tastiness', source=tastiness_files)
