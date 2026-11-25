#!/usr/bin/env python3
"""Generate wasm-api.ttf and wasm-start.ttf, the fonts test-wasm.c shapes with.

Each is Roboto-Regular.abc.ttf with a Wasm table compiled from the .cc file
of the same name.  Needs clang with the wasm32 target, wasm-ld, and fontTools.
"""

import subprocess
import tempfile
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables.DefaultTable import DefaultTable

here = Path(__file__).resolve().parent
src = here.parents[2] / "src"



def generate(name, *flags):
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
                str(here / (name + ".cc")),
                "-o",
                str(module),
            ],
            check=True,
        )

        font = TTFont(here / "Roboto-Regular.abc.ttf")
        table = DefaultTable("Wasm")
        table.data = module.read_bytes()
        font["Wasm"] = table
        font.save(here / (name + ".ttf"))


generate("wasm-api")
# The runtime calls this export when it makes an instance.
generate("wasm-start", "-Wl,--export=__wasm_call_ctors")
