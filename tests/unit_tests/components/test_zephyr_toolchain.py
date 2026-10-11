"""Unit tests for esphome.components.zephyr.toolchain."""

from pathlib import Path

from esphome.build_helpers.native import ANALYSIS_TOOLCHAIN_MODULES
from esphome.components.zephyr import toolchain
from esphome.const import Toolchain
from esphome.core import CORE


def _app_dir(tmp_path: Path) -> Path:
    CORE.build_path = tmp_path
    app = tmp_path / ".west_build" / "zephyr"
    app.mkdir(parents=True)
    return app


def test_tools_come_from_the_build(tmp_path: Path) -> None:
    """The per-architecture binutils the build used, not a guess."""
    app = _app_dir(tmp_path)
    (app / "CMakeCache.txt").write_text(
        "CMAKE_OBJDUMP:FILEPATH=/sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump\n"
        "CMAKE_READELF:FILEPATH=/sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-readelf\n"
    )
    assert toolchain.get_objdump_path() == Path(
        "/sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump"
    )
    assert toolchain.get_readelf_path() == Path(
        "/sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-readelf"
    )
    assert toolchain.get_elf_path() == app / "zephyr" / "zephyr.elf"


def test_tools_missing_before_configure(tmp_path: Path) -> None:
    """Without a configured build the paths do not exist, so the caller reports it."""
    _app_dir(tmp_path)
    assert not toolchain.get_objdump_path().exists()
    assert not toolchain.get_readelf_path().exists()


def test_registered_for_sdk_zephyr() -> None:
    assert (
        ANALYSIS_TOOLCHAIN_MODULES[("zephyr", Toolchain.SDK_ZEPHYR)]
        == "esphome.components.zephyr.toolchain"
    )
