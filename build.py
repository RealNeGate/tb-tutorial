import glob
import os
import platform
import subprocess
import argparse

parser = argparse.ArgumentParser(description='Compiles Cuik')
parser.add_argument('--opt', action='store_true', help='runs optimize on compiled source')

args = parser.parse_args()
if args.opt:
	subprocess.check_call(['build.py', 'x64', '--opt'], shell=True, cwd="tilde-backend")
else:
	subprocess.check_call(['build.py', 'x64'], shell=True, cwd="tilde-backend")

# link everything together
ninja = open('build.ninja', 'w')
ldflags = ""

cflags = "-g -Wall -Werror -Wno-unused-function"
cflags += " -I tilde-backend/include"

if args.opt:
	cflags += " -O2 -DNDEBUG"

# windows' CRT doesn't support c11 threads so we provide a fallback
if platform.system() == "Windows":
	exe_ext = ".exe"
	cflags += " -D_CRT_SECURE_NO_WARNINGS -D_DLL"
	ldflags += " -nodefaultlibs -lmsvcrt -lvcruntime -lucrt"
else:
	exe_ext = ""

# write some rules
ninja.write(f"""
cflags = {cflags}
ldflags = {ldflags}

rule cc
  depfile = $out.d
  command = clang $in $cflags -MD -MF $out.d -c -o $out
  description = CC $in $out

rule link
  command = clang $in $ldflags -g -o $out
  description = LINK $out

""")

# compile source files
objs = []
list = glob.glob("src/*.c")

for f in list:
	obj = os.path.basename(f).replace('.c', '.o')
	ninja.write(f"build bin/{obj}: cc {f}\n")
	objs.append("bin/"+obj)

ninja.write(f"build tutorial{exe_ext}: link {' '.join(objs)} tilde-backend/tildebackend.lib\n")
ninja.close()

subprocess.call(['ninja'])
