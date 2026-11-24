import os
import re
import shutil
import subprocess

# harfbuzz-icu links to libstdc++ because icu does, and harfbuzz-cairo links transitively
# to libxml2 which on some platforms is also linked against icu
CHECK_LIBSTDCXX_LIBS = [
    "harfbuzz",
    "harfbuzz-subset",
    "harfbuzz-raster",
    "harfbuzz-vector",
    "harfbuzz-gobject",
]

CHECK_SYMBOL_LIBS = CHECK_LIBSTDCXX_LIBS + [
    "harfbuzz-cairo",
    "harfbuzz-icu",
]


def find_shared_libraries(builddir, soname):
    """The shared libraries of that name in builddir: ELF, Mach-O, and
    Cygwin's DLL, which is named with the version."""
    for name in ["lib%s.so", "lib%s.dylib", "cyg%s-0.dll"]:
        path = os.path.join(builddir, name % soname)
        if os.path.exists(path):
            yield path


def find_objdump():
    return os.getenv("OBJDUMP") or shutil.which("objdump") or shutil.which("llvm-objdump")


def _dll_headers(objdump, dll):
    return subprocess.check_output(objdump.split() + ["-p", dll]).decode("utf-8")


def dll_exports(objdump, dll):
    """The names in the export table of a DLL, sorted. nm does not read it."""
    names = []
    in_table = False
    for line in _dll_headers(objdump, dll).splitlines():
        if re.match(r"\[Ordinal/Name Pointer\] Table|\s*Ordinal\s+RVA\s+Name$", line):
            in_table = True
        elif not line.strip():
            in_table = False
        elif in_table:
            # GNU objdump: "[   0] +base[   1]  0000 hb_blob_create",
            # or "[   0] hb_blob_create" in older versions.
            # llvm-objdump: "1  0x28680  hb_blob_create".
            m = re.match(
                r"\s*(?:\[\s*\d+\](?: \+base\[\s*\d+\]\s+[0-9a-f]+)?|\d+\s+0x[0-9a-f]+)\s+(\S+)$",
                line,
            )
            if m:
                names.append(m.group(1))
    return sorted(names)


def dll_imports(objdump, dll):
    """The names of the DLLs that a DLL imports from."""
    return re.findall(r"^\s+DLL Name: (\S+)$", _dll_headers(objdump, dll), re.MULTILINE)
