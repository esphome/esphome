"""clang-tidy idedata via the native ESP-IDF toolchain (WIP).

Instead of PlatformIO's ``pio run -t idedata``, produce idedata from a native
ESP-IDF build's ``compile_commands.json`` (CMAKE_EXPORT_COMPILE_COMMANDS),
transformed by :mod:`esphome.espidf.idedata` -- the same code path
``esphome compile`` uses.

Sourcing the compile DB (in priority order):

1. ``ESPHOME_IDF_COMPILE_COMMANDS`` -- explicit path to a build's
   ``compile_commands.json`` (fast iteration; reuse an existing build).
2. ``ESPHOME_TIDY_CONFIG`` -- an ESPHome YAML (esp32, ``toolchain: esp-idf``)
   that is compiled with ``esphome compile`` to generate the compile DB.

For full clang-tidy coverage the config must pull in every component (the
analog of the PlatformIO all-include env) so every component's include path is
present; ``esphome/core/defines.h`` enables all ``USE_*``.
"""

import os
from pathlib import Path
import subprocess

from esphome.espidf.idedata import idedata_from_compile_commands


def _compile_commands_from_config(config: str, temp_folder: str) -> Path:
    """Compile an ESPHome config with the IDF toolchain and return its compile DB."""
    subprocess.run(["esphome", "compile", config], check=True)
    # esphome writes <config_dir>/.esphome/build/<name>/build/compile_commands.json.
    config_dir = Path(config).resolve().parent
    builds = config_dir / ".esphome" / "build"
    candidates = sorted(
        builds.glob("*/build/compile_commands.json"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise RuntimeError(f"no compile_commands.json found under {builds}")
    return candidates[0]


def load_idedata(environment: str, temp_folder: str, platformio_ini: str) -> dict:
    explicit = os.environ.get("ESPHOME_IDF_COMPILE_COMMANDS")
    if explicit:
        compile_commands = Path(explicit)
    elif config := os.environ.get("ESPHOME_TIDY_CONFIG"):
        compile_commands = _compile_commands_from_config(config, temp_folder)
    else:
        raise RuntimeError(
            "native IDF tidy needs ESPHOME_IDF_COMPILE_COMMANDS (path to a "
            "build's compile_commands.json) or ESPHOME_TIDY_CONFIG (an esp32 "
            "esp-idf YAML to compile)"
        )
    if not compile_commands.is_file():
        raise RuntimeError(f"compile_commands.json not found: {compile_commands}")
    return idedata_from_compile_commands(compile_commands)
