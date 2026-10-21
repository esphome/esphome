"""Binutils and the linked image of a platform: zephyr build, for memory analysis."""

from pathlib import Path

from esphome.core import CORE


def _app_build_dir() -> Path:
    return CORE.relative_build_path(".west_build", "zephyr")


def _cmake_tool(key: str) -> Path:
    """The tool the build itself used, per architecture (host binutils on
    native_sim); a guessed one could not read the target ELF."""
    cache = _app_build_dir() / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith(f"{key}:"):
                return Path(line.split("=", 1)[1])
    # Not configured yet: a missing path the caller reports
    return _app_build_dir() / key


def get_objdump_path() -> Path:
    return _cmake_tool("CMAKE_OBJDUMP")


def get_readelf_path() -> Path:
    return _cmake_tool("CMAKE_READELF")


def get_elf_path() -> Path:
    return _app_build_dir() / "zephyr" / "zephyr.elf"
