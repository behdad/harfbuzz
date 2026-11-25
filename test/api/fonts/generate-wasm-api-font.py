#!/usr/bin/env python3
"""Generate the fonts test-wasm.c shapes with.

Each is Roboto-Regular.abc.ttf, or another font here, with a Wasm table
compiled from a .cc file here.  Needs clang with the wasm32 target,
wasm-ld, and fontTools.
"""

import subprocess
import tempfile
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables.DefaultTable import DefaultTable

here = Path(__file__).resolve().parent
src = here.parents[2] / "src"



def leb128(value):
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def read_leb128(data, pos):
    value = shift = 0
    while True:
        byte = data[pos]
        pos += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return value, pos


def add_start_section(data, export):
    """Returns the module with the exported function as its start function."""
    out = bytearray(data[:8])
    pos = 8
    index = None
    while pos < len(data):
        section = data[pos]
        size, body = read_leb128(data, pos + 1)
        out += data[pos : body + size]
        if section == 7:
            count, at = read_leb128(data, body)
            for _ in range(count):
                length, at = read_leb128(data, at)
                name = data[at : at + length]
                kind = data[at + length]
                value, at = read_leb128(data, at + length + 1)
                if kind == 0 and name == export.encode():
                    index = value
            assert index is not None
            # The start section follows the export section.
            payload = leb128(index)
            out += bytes([8]) + leb128(len(payload)) + payload
        pos = body + size
    return bytes(out)


def generate(name, *flags, source=None, start=None, base="Roboto-Regular.abc.ttf"):
    with tempfile.TemporaryDirectory() as tmp:
        module = Path(tmp) / (name + ".wasm")
        subprocess.run(
            [
                "clang",
                "--target=wasm32",
                "-O1",
                "-ffreestanding",
                "-nostdlib",
                "-fvisibility=hidden",
                "-Wl,--no-entry",
                "-Wl,--allow-undefined",
                "-Wl,--strip-all",
                *flags,
                "-I",
                str(src),
                str(here / ((source or name) + ".cc")),
                "-o",
                str(module),
            ],
            check=True,
        )

        font = TTFont(here / base)
        table = DefaultTable("Wasm")
        table.data = module.read_bytes()
        if start:
            table.data = add_start_section(table.data, start)
        font["Wasm"] = table
        font.save(here / (name + ".ttf"))


generate("wasm-api")
# The same module in a variable font, for the font's coordinates.
generate("wasm-coords", source="wasm-api", base="glyf-default-instance-bounds.ttf")
# The runtime calls this export when it makes an instance.
generate("wasm-start", "-Wl,--export=__wasm_call_ctors")
generate("wasm-import", "-DIMPORT", source="wasm-refused")
generate("wasm-import-libc", "-DIMPORT_LIBC", source="wasm-refused")
generate("wasm-allocator", "-DALLOCATOR", source="wasm-refused")
generate("wasm-start-section", "-DSTART", source="wasm-refused", start="early")
generate("wasm-fixed-memory", "-Wl,--max-memory=1048576")
generate("wasm-allocator-grow", "-DGROW", source="wasm-own-allocator")
generate("wasm-allocator-reenter", "-DREENTER", source="wasm-own-allocator")
generate("wasm-allocator-arena", "-DARENA", source="wasm-own-allocator")
# Each with a maximum for its memory, so that one instance serves buffers
# of any length whatever the runtime makes of a module without one.
for name, *flags in [
    ("wasm-leak",),
    ("wasm-leak-own", "-DOWN"),
    ("wasm-leak-spin", "-DOWN", "-DSPIN"),
    ("wasm-keep", "-DKEEP"),
    ("wasm-keep-without", "-DKEEP", "-DWITHOUT"),
    ("wasm-hold", "-DHOLD"),
    ("wasm-ask", "-DASK=200000"),
    ("wasm-ask-two", "-DASK=60000"),
    ("wasm-ask-both", "-DASK=30000", "-DBOTH"),
    ("wasm-fill", "-DFILL"),
    ("wasm-leak-ask", "-DASK=60000", "-DLEAK"),
    ("wasm-leak-late", "-DLATE"),
]:
    generate(name, "-Wl,--max-memory=1048576", *flags, source="wasm-leak")
