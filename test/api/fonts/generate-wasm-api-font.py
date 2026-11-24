#!/usr/bin/env python3
"""Generate wasm-api.ttf, the font test-wasm.c shapes with.

It is Roboto-Regular.abc.ttf with a Wasm table compiled from wasm-api.cc.
Needs clang with the wasm32 target, wasm-ld, and fontTools.
"""

import subprocess
import tempfile
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables.DefaultTable import DefaultTable

here = Path(__file__).resolve().parent
src = here.parents[2] / "src"

with tempfile.TemporaryDirectory() as tmp:
    module = Path(tmp) / "wasm-api.wasm"
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
            "-I",
            str(src),
            str(here / "wasm-api.cc"),
            "-o",
            str(module),
        ],
        check=True,
    )

    font = TTFont(here / "Roboto-Regular.abc.ttf")
    table = DefaultTable("Wasm")
    table.data = module.read_bytes()
    font["Wasm"] = table
    font.save(here / "wasm-api.ttf")
