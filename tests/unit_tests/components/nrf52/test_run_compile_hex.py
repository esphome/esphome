"""nrf52 sdk-nrf build: the hex file used for the Adafruit UF2 and DFU package."""

from collections.abc import Iterator
from pathlib import Path
from unittest.mock import Mock, patch

import pytest

from esphome.components import nrf52
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT_NRF52_SD140_V7
from esphome.components.zephyr.const import KEY_BOARD, KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE


@pytest.fixture
def build_dir(tmp_path: Path) -> Path:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    CORE.toolchain = Toolchain.SDK_NRF
    return CORE.relative_pioenvs_path(CORE.name)


@pytest.fixture
def run_cmd(tmp_path: Path) -> Iterator[Mock]:
    with (
        patch.object(nrf52, "check_and_install"),
        patch.object(nrf52, "_generate_cmake_lists", return_value=False),
        patch.object(
            nrf52,
            "get_build_paths",
            return_value={"python_executable": "python3", "framework_path": tmp_path},
        ),
        patch.object(nrf52, "get_build_env", return_value={}),
        patch.object(
            nrf52,
            "zephyr_data",
            return_value={
                KEY_BOARD: "board",
                KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
            },
        ),
        patch.object(nrf52.pch, "pch_enabled", return_value=False),
        patch.object(nrf52, "run_command_ok", return_value=True) as run,
    ):
        yield run


def _uf2_hex(run_cmd: Mock) -> str:
    cmd = next(c.args[0] for c in run_cmd.call_args_list if "-f" in c.args[0])
    return cmd[-1]


def _dfu_hex(run_cmd: Mock) -> str:
    cmd = next(c.args[0] for c in run_cmd.call_args_list if "genpkg" in c.args[0])
    return cmd[cmd.index("--application") + 1]


def _build(build_dir: Path, version: cv.Version, outputs: list[str]) -> None:
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: version}
    (build_dir / "zephyr" / "zephyr").mkdir(parents=True)
    # Without a CMake cache run_compile wipes the build dir first
    for name in ["CMakeCache.txt", *outputs]:
        (build_dir / name).write_text("")
    nrf52.run_compile(None, {})


def test_uf2_and_dfu_use_merged_hex(build_dir: Path, run_cmd: Mock) -> None:
    _build(
        build_dir,
        cv.Version(2, 9, 2),
        ["merged.hex", "zephyr/zephyr/zephyr.hex"],
    )
    merged = str(build_dir / "zephyr" / "merged.hex")
    assert _uf2_hex(run_cmd) == merged
    assert _dfu_hex(run_cmd) == merged


def test_uf2_and_dfu_fall_back_to_zephyr_hex(build_dir: Path, run_cmd: Mock) -> None:
    """The nRF Connect SDK 3.4.0 build produces no merged.hex."""
    _build(build_dir, cv.Version(3, 4, 0), ["zephyr/zephyr/zephyr.hex"])
    app_hex = build_dir / "zephyr" / "zephyr.hex"
    assert app_hex.is_file()
    assert _uf2_hex(run_cmd) == str(app_hex)
    assert _dfu_hex(run_cmd) == str(app_hex)


def test_older_sdk_does_not_fall_back_to_zephyr_hex(
    build_dir: Path, run_cmd: Mock
) -> None:
    """Before SDK 3.4.0 a missing merged.hex must not be replaced by zephyr.hex."""
    _build(build_dir, cv.Version(3, 2, 0), ["zephyr/zephyr/zephyr.hex"])
    assert not any("-f" in c.args[0] for c in run_cmd.call_args_list)
    assert _dfu_hex(run_cmd) == str(build_dir / "zephyr" / "merged.hex")


def test_sdk_3_4_0_ignores_a_stale_merged_hex(build_dir: Path, run_cmd: Mock) -> None:
    """A merged.hex left by an older SDK build must not be packaged."""
    _build(
        build_dir,
        cv.Version(3, 4, 0),
        ["zephyr/merged.hex", "zephyr/zephyr/zephyr.hex"],
    )
    app_hex = str(build_dir / "zephyr" / "zephyr.hex")
    assert _uf2_hex(run_cmd) == app_hex
    assert _dfu_hex(run_cmd) == app_hex
