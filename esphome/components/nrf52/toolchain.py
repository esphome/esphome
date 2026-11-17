"""The native build backend of the nRF52 sdk-nrf toolchain.

The build itself runs from the platform's ``run_compile`` hook; this module
gives the binutils, the linked image and idedata of that build.
"""

from pathlib import Path

import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE

from .framework import toolchain_tool


def get_objdump_path() -> Path:
    return toolchain_tool("objdump")


def get_readelf_path() -> Path:
    return toolchain_tool("readelf")


def get_elf_path() -> Path:
    """The linked Zephyr image, at the layout the configured SDK version writes.

    Chosen by version rather than by probing so a leftover image from another
    SDK layout is never analyzed in place of the current build.
    """
    if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] < cv.Version(2, 9, 2):
        return CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr.elf")
    return CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr", "zephyr.elf")


def get_idedata() -> dict | None:
    """Derive idedata from the app's compile_commands.json, next to its
    ``zephyr/`` output directory. Returns None if nothing has been built yet."""
    from esphome.build_helpers.idedata import load_or_build_idedata

    elf = get_elf_path()
    # No launcher: Zephyr's CMake leaves ccache out of the compile database
    return load_or_build_idedata(
        elf.parent.parent / "compile_commands.json",
        elf,
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
    )
