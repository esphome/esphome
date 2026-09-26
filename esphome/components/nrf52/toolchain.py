"""Binutils and the linked image of an nRF52 sdk-nrf build, for memory analysis."""

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
