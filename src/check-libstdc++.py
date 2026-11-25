#!/usr/bin/env python3

import os
import re
import shutil
import subprocess
import sys

from check_helpers import CHECK_LIBSTDCXX_LIBS, CHECK_SYMBOL_LIBS, dll_imports, find_objdump, find_shared_libraries

srcdir = sys.argv[1]
base_srcdir = sys.argv[2]
builddir = sys.argv[3]

os.chdir(srcdir)

ldd = os.getenv("LDD", shutil.which("ldd"))
readelf = os.getenv("READELF") or shutil.which("readelf") or shutil.which("llvm-readelf")
objdump = find_objdump()
if not ldd:
    otool = os.getenv("OTOOL", shutil.which("otool"))
    if otool:
        ldd = otool + " -L"
    else:
        print("check-libstdc++.py: 'ldd' not found; skipping test")
        sys.exit(77)

stat = 0
tested = False

check_libs = CHECK_SYMBOL_LIBS
if not readelf:
    check_libs = CHECK_LIBSTDCXX_LIBS

for soname in check_libs:
    for so in find_shared_libraries(builddir, soname):
        if so.endswith(".dll") and not objdump:
            print("'objdump' not found; skipping %s" % so)
            continue

        print("Checking that we are not linking to libstdc++ or libc++ in %s" % so)
        if so.endswith(".dll"):
            ldd_result = "\n".join(dll_imports(objdump, so)).encode("utf-8")
        elif readelf:
            dynamic = subprocess.check_output([readelf, "-d", so])
            ldd_result = b"\n".join(line for line in dynamic.splitlines() if b"(NEEDED)" in line)
        else:
            ldd_result = subprocess.check_output(ldd.split() + [so])
        # Cygwin has cygstdc++-6.dll and cygc++-1.dll
        if re.search(rb"(lib|cyg)(std)?c\+\+", ldd_result):
            print("Ouch, %s is linked to libstdc++ or libc++" % so)
            stat = 1

        tested = True

if not tested:
    print("check-libstdc++.py: libharfbuzz shared library not found; skipping test")
    sys.exit(77)

sys.exit(stat)
