"""Tests for the XIAO STM32C5's TinyUF2 support in the stm32c5 variant."""

from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.zephyr import reset_over_usb, upload_program
from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr.variants import stm32c5
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_ZEPHYR,
)
from esphome.core import CORE, EsphomeError
from esphome.upload_targets import PortType
from tests.unit_tests.components.zephyr.zephyr_state import empty_zephyr_data

XIAO = "xiao_stm32c5"
NUCLEO = "nucleo_c5a3zg"
PORT = "/dev/ttyACM0"
_DTS = "esphome.components.zephyr.dts_lookup"


def _state(tmp_path: Path, board: str = XIAO) -> None:
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(variant="STM32C5", board=board)
    CORE.data[KEY_CORE] = {
        KEY_FRAMEWORK_VERSION: "4.5.0-rc3",
        KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR,
    }
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"


def _write_uf2() -> None:
    uf2 = CORE.relative_build_path(".west_build/zephyr/zephyr/zephyr.uf2")
    uf2.parent.mkdir(parents=True)
    uf2.write_bytes(b"UF2 blocks")


@pytest.mark.parametrize(("board", "expected"), [(XIAO, True), (NUCLEO, False)])
def test_uses_tinyuf2(tmp_path: Path, board: str, expected: bool) -> None:
    _state(tmp_path, board)
    assert stm32c5.uses_tinyuf2() is expected


@pytest.mark.parametrize(("board", "declared"), [(XIAO, True), (NUCLEO, False)])
def test_config_schema_declares_the_touch_only_on_the_xiao(
    board: str, declared: bool
) -> None:
    config = {CONF_BOARD: board, CONF_ADVANCED: {}}
    stm32c5._xiao_config_schema(config)
    assert (stm32c5.CONF_TINYUF2_TOUCH in config[CONF_ADVANCED]) is declared


@pytest.mark.parametrize(("board", "built"), [(XIAO, True), (NUCLEO, False)])
def test_to_code_builds_uf2_and_touch_only_on_the_xiao(board: str, built: bool) -> None:
    with (
        patch("esphome.components.zephyr.zephyr_add_prj_conf") as mock_conf,
        patch("esphome.components.zephyr.zephyr_add_overlay"),
        patch(f"{_DTS}.get_ck48_node_label", return_value="clk48"),
        patch(f"{_DTS}.has_pinctrl_configured", return_value=True),
        patch.object(stm32c5.cg, "add_define") as mock_define,
        patch.object(CORE, "add_job") as mock_job,
    ):
        stm32c5._xiao_to_code({CONF_BOARD: board, CONF_ADVANCED: {}})
    if built:
        mock_conf.assert_any_call("BUILD_OUTPUT_UF2", True)
        mock_conf.assert_any_call("BUILD_OUTPUT_UF2_FAMILY_ID", "0x00c5c5c5")
        mock_define.assert_called_once_with("USE_ZEPHYR_TINYUF2_TOUCH")
        mock_job.assert_called_once()
    else:
        mock_conf.assert_not_called()
        mock_define.assert_not_called()
        mock_job.assert_not_called()


@pytest.mark.parametrize(
    ("ck48_label", "usb_pins", "expected"),
    [
        (None, False, ["&clk48", "&usb"]),
        ("clk48", False, ["&usb"]),
        (None, True, ["&clk48"]),
        ("clk48", True, []),
    ],
)
def test_to_code_adds_usb_clock_and_pins_only_while_the_board_lacks_them(
    ck48_label: str | None, usb_pins: bool, expected: list[str]
) -> None:
    with (
        patch("esphome.components.zephyr.zephyr_add_prj_conf"),
        patch("esphome.components.zephyr.zephyr_add_overlay") as mock_overlay,
        patch(f"{_DTS}.get_ck48_node_label", return_value=ck48_label),
        patch(f"{_DTS}.has_pinctrl_configured", return_value=usb_pins),
        patch.object(stm32c5.cg, "add_define"),
        patch.object(CORE, "add_job"),
    ):
        stm32c5._xiao_to_code({CONF_BOARD: XIAO, CONF_ADVANCED: {}})
    added = [call.args[0].split()[0] for call in mock_overlay.call_args_list]
    assert added == expected


def test_find_uf2_disk() -> None:
    disk = Path("/dev/disk/by-id/usb-Adafruit_UF2_Bootloader_123-0:0")
    with patch.object(Path, "glob", return_value=iter([disk])) as mock_glob:
        assert stm32c5._find_uf2_disk() == disk
    mock_glob.assert_called_once_with("usb-Adafruit_UF2_Bootloader_*")


def test_upload_writes_to_a_bootloader_already_waiting(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_uf2()
    disk = tmp_path / "sdb"
    with (
        patch.object(stm32c5, "_find_uf2_disk", return_value=disk),
        patch("serial.Serial") as mock_serial,
    ):
        assert stm32c5.upload_program(PORT) is True
    mock_serial.assert_not_called()
    assert disk.read_bytes() == b"UF2 blocks"


def test_upload_touches_at_1200_baud_then_writes(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_uf2()
    disk = tmp_path / "sdb"
    with (
        patch.object(stm32c5, "_find_uf2_disk", side_effect=[None, None, disk]),
        patch("serial.Serial") as mock_serial,
        patch("time.sleep"),
    ):
        assert stm32c5.upload_program(PORT) is True
    mock_serial.assert_called_once_with(PORT, baudrate=1200)
    assert disk.read_bytes() == b"UF2 blocks"


def test_upload_raises_when_the_bootloader_never_appears(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_uf2()
    ticks = iter(range(0, 1000, 6))
    with (
        patch.object(stm32c5, "_find_uf2_disk", return_value=None),
        patch("serial.Serial"),
        patch("time.sleep"),
        patch("time.monotonic", side_effect=lambda: next(ticks)),
        pytest.raises(EsphomeError, match="bootloader mode"),
    ):
        stm32c5.upload_program(PORT)


def test_upload_points_at_udev_without_write_access(tmp_path: Path) -> None:
    _state(tmp_path)
    _write_uf2()
    disk = MagicMock(open=MagicMock(side_effect=PermissionError))
    with (
        patch.object(stm32c5, "_find_uf2_disk", return_value=disk),
        pytest.raises(EsphomeError, match="udev rule"),
    ):
        stm32c5.upload_program(PORT)


def test_upload_program_uses_tinyuf2_on_the_xiao(tmp_path: Path) -> None:
    _state(tmp_path)
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.components.zephyr.build_zephyr.check_bootloader_built"),
        patch.object(stm32c5, "upload_program", return_value=True) as mock_upload,
    ):
        assert upload_program({PLATFORM_ZEPHYR: {CONF_FRAMEWORK: {}}}, object(), PORT)
    mock_upload.assert_called_once_with(PORT)


@pytest.mark.parametrize(("board", "handled"), [(XIAO, True), (NUCLEO, False)])
def test_reset_over_usb_touches_the_xiao_at_2001_baud(
    tmp_path: Path, board: str, handled: bool
) -> None:
    _state(tmp_path, board)
    ports = MagicMock(
        side_effect=[[SimpleNamespace(path=PORT)], [], [SimpleNamespace(path=PORT)]]
    )
    config = {PLATFORM_ZEPHYR: {"variant": "STM32C5", "advanced": {}}}
    with (
        patch("serial.Serial") as mock_serial,
        patch("esphome.util.get_serial_ports", ports),
        patch("time.sleep"),
    ):
        assert reset_over_usb(config, PORT) is handled
    if handled:
        assert mock_serial.call_args.kwargs["baudrate"] == 2001
    else:
        mock_serial.assert_not_called()
