"""Unit tests for esphome.components.zephyr.toolchain."""

from pathlib import Path
from unittest.mock import patch

from esphome.build_helpers.native import NATIVE_TOOLCHAIN_MODULES
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
        NATIVE_TOOLCHAIN_MODULES[("zephyr", Toolchain.SDK_ZEPHYR)]
        == "esphome.components.zephyr.toolchain"
    )


def test_get_idedata_reads_the_app_compile_database(tmp_path: Path) -> None:
    """The compile database and ELF come from the west app build dir."""
    app = _app_dir(tmp_path)
    CORE.config_path = tmp_path / "test.yaml"
    CORE.name = "livingroom"
    with patch(
        "esphome.build_helpers.idedata.load_or_build_idedata",
        return_value={"prog_path": "x"},
    ) as load:
        assert toolchain.get_idedata() == {"prog_path": "x"}

    compile_commands, elf, _cache = load.call_args.args
    assert compile_commands == app / "compile_commands.json"
    assert elf == app / "zephyr" / "zephyr.elf"
