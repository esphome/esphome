"""clang-tidy idedata via the native ESP-IDF toolchain (WIP).

Instead of PlatformIO's ``pio run -t idedata``, produce idedata from a native
ESP-IDF build's ``compile_commands.json`` (CMAKE_EXPORT_COMPILE_COMMANDS),
transformed by :mod:`esphome.espidf.idedata`.

The compile DB is generated with a *reconfigure* (CMake configure only, no
compilation): run ESPHome codegen for a config, then mirror ``run_compile``'s
pre-build steps -- ``write_project`` + ``idf.py reconfigure`` -- which is fast
and emits ``build/compile_commands.json``.

Sourcing (priority order):

1. ``ESPHOME_IDF_COMPILE_COMMANDS`` -- explicit path to an existing build's
   ``compile_commands.json`` (skips generation; fast iteration).
2. ``ESPHOME_TIDY_CONFIG`` -- an ESPHome YAML (esp32, ``toolchain: esp-idf``)
   to generate + reconfigure.

For full clang-tidy coverage the config must pull in every component (the
analog of the PlatformIO all-include env); ``esphome/core/defines.h`` enables
all ``USE_*``.
"""

import os
from pathlib import Path

from esphome.espidf.idedata import idedata_from_build


def _generate_and_reconfigure(config: str) -> Path:
    """Run ESPHome codegen + ``idf.py reconfigure`` (no build) for ``config``.

    Mirrors the pre-build phase of ``espidf.toolchain.run_compile``: codegen,
    a minimal-REQUIRES configure to discover managed components, then the full
    project + a reconfigure that emits the complete compile_commands.json.
    """
    from esphome.__main__ import write_cpp
    from esphome.build_gen.espidf import write_project
    import esphome.config as config_module
    from esphome.core import CORE
    from esphome.espidf import toolchain

    CORE.config_path = Path(config)
    parsed = config_module.read_config({})
    if parsed is None:
        raise RuntimeError(f"invalid ESPHome config: {config}")
    CORE.config = parsed

    if write_cpp(parsed) != 0:
        raise RuntimeError("ESPHome codegen (write_cpp) failed")

    # Two-phase configure (discover managed components, then full REQUIRES),
    # but skip the build -- reconfigure alone emits compile_commands.json.
    write_project(minimal=True)
    if toolchain.run_reconfigure() != 0:
        raise RuntimeError("idf.py reconfigure (discovery) failed")
    write_project(minimal=False)
    if toolchain.run_reconfigure() != 0:
        raise RuntimeError("idf.py reconfigure failed")

    return CORE.relative_build_path("build", "compile_commands.json")


def load_idedata(environment: str, temp_folder: str, platformio_ini: str) -> dict:
    if explicit := os.environ.get("ESPHOME_IDF_COMPILE_COMMANDS"):
        compile_commands = Path(explicit)
    elif config := os.environ.get("ESPHOME_TIDY_CONFIG"):
        compile_commands = _generate_and_reconfigure(config)
    else:
        raise RuntimeError(
            "native IDF tidy needs ESPHOME_IDF_COMPILE_COMMANDS (path to a "
            "build's compile_commands.json) or ESPHOME_TIDY_CONFIG (an esp32 "
            "esp-idf YAML to generate + reconfigure)"
        )
    if not compile_commands.is_file():
        raise RuntimeError(f"compile_commands.json not found: {compile_commands}")
    return idedata_from_build(compile_commands)
