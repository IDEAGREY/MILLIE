#!/usr/bin/env python3
"""Compile the app-only image for the confirmed single-USB MILLIE CYD. Never flashes."""
from pathlib import Path
import subprocess,shutil
root=Path(__file__).resolve().parent
cli=shutil.which('arduino-cli')
if not cli:raise SystemExit('Install arduino-cli and the dependencies listed in README.md first.')
subprocess.run([cli,'compile','--fqbn','esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M,FlashMode=dio','--build-property',f'compiler.cpp.extra_flags=-include "{root / "CYD_Setup.h"}"','--build-path',str(root/'build'),'--output-dir',str(root/'firmware'),str(root/'millie-standalone')],check=True)
