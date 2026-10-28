"""The native build backend of the platform: zephyr sdk-zephyr toolchain.

The build itself runs from the platform's ``run_compile`` hook; this module
gives the binutils, the linked image and idedata of that build.
"""

from pathlib import Path

from esphome.core import CORE


def _app_build_dir() -> Path:
    return CORE.relative_build_path(".west_build", "zephyr")


def _cmake_tool(key: str) -> Path:
    """The tool the build itself used, per architecture; a guessed one could
    not read the target ELF."""
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


def get_idedata() -> dict | None:
    """Derive idedata from the app's compile_commands.json. Returns None if
    nothing has been built yet."""
    from esphome.build_helpers.idedata import load_or_build_idedata

    # No launcher: Zephyr's CMake leaves ccache out of the compile database
    return load_or_build_idedata(
        _app_build_dir() / "compile_commands.json",
        get_elf_path(),
        CORE.relative_internal_path("idedata", f"{CORE.name}.json"),
    )
