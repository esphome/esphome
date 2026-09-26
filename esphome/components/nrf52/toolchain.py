"""Binutils and the linked image of an nRF52 sdk-nrf build, for memory analysis."""

from pathlib import Path

from esphome.core import CORE

from .framework import toolchain_tool


def get_objdump_path() -> Path:
    return toolchain_tool("objdump")


def get_readelf_path() -> Path:
    return toolchain_tool("readelf")


def get_elf_path() -> Path:
    """The linked Zephyr image; the SDK nests it one level deeper from 2.9.2."""
    nested = CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr", "zephyr.elf")
    if nested.is_file():
        return nested
    return CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr.elf")
