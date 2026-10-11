"""Tests for the host side of the nRF52 `advanced: dfu:` option."""

from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome.components.zephyr import reset_over_usb, upload_program
from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr.variants import nrf52
from esphome.const import (
    CONF_FRAMEWORK,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_ZEPHYR,
)
from esphome.core import CORE, EsphomeError
from esphome.upload_targets import PortType
from tests.unit_tests.components.zephyr_state import empty_zephyr_data

V6 = "adafruit_nrf52_sd140_v6"
V7 = "adafruit_nrf52_sd140_v7"


def _nrf52_state(tmp_path: Path, bootloader: str = V6) -> None:
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(variant="NRF52", bootloader=bootloader)
    CORE.data[KEY_CORE] = {
        KEY_FRAMEWORK_VERSION: "4.4.1",
        KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR,
    }
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"


@pytest.mark.parametrize(
    ("bootloader", "expected"), [(V6, True), (V7, True), ("mcuboot", False)]
)
def test_uses_serial_dfu(tmp_path: Path, bootloader: str, expected: bool) -> None:
    _nrf52_state(tmp_path, bootloader)
    assert nrf52.uses_serial_dfu() is expected


def test_ensure_nrfutil_skips_install_when_present() -> None:
    with (
        patch("subprocess.run", return_value=SimpleNamespace(returncode=0)),
        patch.object(nrf52, "run_command_ok") as mock_run,
    ):
        nrf52._ensure_nrfutil(Path("python"), {})
    mock_run.assert_not_called()


def test_ensure_nrfutil_installs_when_missing() -> None:
    with (
        patch("subprocess.run", return_value=SimpleNamespace(returncode=1)),
        patch.object(nrf52, "run_command_ok", return_value=True) as mock_run,
    ):
        nrf52._ensure_nrfutil(Path("python"), {})
    assert "pip" in mock_run.call_args[0][0]


def test_ensure_nrfutil_raises_when_install_fails() -> None:
    with (
        patch("subprocess.run", return_value=SimpleNamespace(returncode=1)),
        patch.object(nrf52, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="adafruit-nrfutil"),
    ):
        nrf52._ensure_nrfutil(Path("python"), {})


def test_build_dfu_package_needs_the_hex(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    with pytest.raises(EsphomeError, match="zephyr.hex"):
        nrf52.build_dfu_package(Path("python"), {})


@pytest.mark.parametrize(("bootloader", "sd_req"), [(V6, "0x00B6"), (V7, "0x00CA")])
def test_build_dfu_package_runs_genpkg(
    tmp_path: Path, bootloader: str, sd_req: str
) -> None:
    _nrf52_state(tmp_path, bootloader)
    hex_file = (
        CORE.relative_build_path(".west_build") / "zephyr" / "zephyr" / "zephyr.hex"
    )
    hex_file.parent.mkdir(parents=True)
    hex_file.write_text("")
    with (
        patch.object(nrf52, "_ensure_nrfutil"),
        patch.object(nrf52, "run_command_ok", return_value=True) as mock_run,
    ):
        nrf52.build_dfu_package(Path("python"), {})
    cmd = mock_run.call_args[0][0]
    assert cmd[cmd.index("--sd-req") + 1] == sd_req
    assert cmd[cmd.index("--application") + 1] == str(hex_file)
    assert cmd[-1] == str(CORE.relative_build_path("firmware.zip"))


def test_build_dfu_package_raises_when_genpkg_fails(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    hex_file = (
        CORE.relative_build_path(".west_build") / "zephyr" / "zephyr" / "zephyr.hex"
    )
    hex_file.parent.mkdir(parents=True)
    hex_file.write_text("")
    with (
        patch.object(nrf52, "_ensure_nrfutil"),
        patch.object(nrf52, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="DFU package"),
    ):
        nrf52.build_dfu_package(Path("python"), {})


def _upload_patches(**overrides: object) -> list:
    """Patch everything upload_serial_dfu touches outside this module's own logic."""
    targets = {
        "esphome.components.zephyr.framework_west.check_and_install": {
            "return_value": (Path("python"), Path("framework"), {})
        },
        "esphome.__main__.check_permissions": {},
        "serial.Serial": {},
        "time.sleep": {},
    }
    return [
        patch(name, **{**kwargs, **overrides.get(name, {})})
        for name, kwargs in targets.items()
    ]


def test_upload_serial_dfu_needs_the_package(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    with pytest.raises(EsphomeError, match="compile first"):
        nrf52.upload_serial_dfu("/dev/ttyACM0")


def test_upload_serial_dfu_touches_then_sends_the_package(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    package = CORE.relative_build_path("firmware.zip")
    package.parent.mkdir(parents=True)
    package.write_text("")
    patches = _upload_patches()
    with (
        patches[0],
        patches[1],
        patches[2] as mock_serial,
        patches[3],
        patch.object(nrf52, "_ensure_nrfutil"),
        patch.object(nrf52, "_wait_for_port", return_value=True),
        patch.object(nrf52, "run_command_ok", return_value=True) as mock_run,
    ):
        assert nrf52.upload_serial_dfu("/dev/ttyACM0") is True
    assert mock_serial.call_args.kwargs["baudrate"] == 1200
    cmd = mock_run.call_args[0][0]
    assert cmd[cmd.index("-pkg") + 1] == str(package)
    assert cmd[cmd.index("-p") + 1] == "/dev/ttyACM0"


def test_upload_serial_dfu_raises_when_the_bootloader_port_never_returns(
    tmp_path: Path,
) -> None:
    _nrf52_state(tmp_path)
    package = CORE.relative_build_path("firmware.zip")
    package.parent.mkdir(parents=True)
    package.write_text("")
    patches = _upload_patches()
    with (
        patches[0],
        patches[1],
        patches[2],
        patches[3],
        patch.object(nrf52, "_ensure_nrfutil"),
        patch.object(nrf52, "_wait_for_port", side_effect=[True, False]),
        pytest.raises(EsphomeError, match="did not reappear"),
    ):
        nrf52.upload_serial_dfu("/dev/ttyACM0")


def test_upload_serial_dfu_raises_when_the_transfer_fails(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    package = CORE.relative_build_path("firmware.zip")
    package.parent.mkdir(parents=True)
    package.write_text("")
    patches = _upload_patches()
    with (
        patches[0],
        patches[1],
        patches[2],
        patches[3],
        patch.object(nrf52, "_ensure_nrfutil"),
        patch.object(nrf52, "_wait_for_port", return_value=True),
        patch.object(nrf52, "run_command_ok", return_value=False),
        pytest.raises(EsphomeError, match="serial DFU upload failed"),
    ):
        nrf52.upload_serial_dfu("/dev/ttyACM0")


def test_upload_program_uses_serial_dfu_for_adafruit_nrf52(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.components.zephyr.build_zephyr.check_bootloader_built"),
        patch.object(nrf52, "upload_serial_dfu", return_value=True) as mock_dfu,
    ):
        result = upload_program(
            {PLATFORM_ZEPHYR: {CONF_FRAMEWORK: {}}}, object(), "/dev/ttyACM0"
        )
    assert result is True
    mock_dfu.assert_called_once_with("/dev/ttyACM0")


# ---------------------------------------------------------------------------
# reset_over_usb -- `esphome logs --reset`
# ---------------------------------------------------------------------------


def _reset_config(variant: str, advanced: dict | None = None) -> dict:
    return {PLATFORM_ZEPHYR: {"variant": variant, "advanced": advanced or {}}}


def _ports(*present: bool) -> MagicMock:
    """Serial port lists for successive polls: whether /dev/ttyACM0 is listed in each."""
    return MagicMock(
        side_effect=[
            [SimpleNamespace(path="/dev/ttyACM0")] if p else [] for p in present
        ]
    )


def test_reset_over_usb_ignores_variants_without_a_handler(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(variant="ESP32H2")
    with patch("serial.Serial") as mock_serial:
        assert reset_over_usb(_reset_config("ESP32H2"), "/dev/ttyACM0") is False
    mock_serial.assert_not_called()


def test_reset_over_usb_needs_dfu_on_nrf52(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    with patch("serial.Serial") as mock_serial:
        assert (
            reset_over_usb(_reset_config("NRF52", {"dfu": None}), "/dev/ttyACM0")
            is False
        )
    mock_serial.assert_not_called()


@pytest.mark.parametrize(
    ("variant", "advanced"),
    [("NRF52", {"dfu": {}}), ("RP2040", {}), ("RP2350", {})],
)
def test_reset_over_usb_touches_at_2001_baud(
    tmp_path: Path, variant: str, advanced: dict
) -> None:
    _nrf52_state(tmp_path)
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(variant=variant)
    with (
        patch("serial.Serial") as mock_serial,
        patch("esphome.util.get_serial_ports", _ports(True, False, True)),
        patch("time.sleep"),
    ):
        assert reset_over_usb(_reset_config(variant, advanced), "/dev/ttyACM0") is True
    assert mock_serial.call_args.kwargs["baudrate"] == 2001


def test_reset_over_usb_gives_up_when_the_port_never_leaves(tmp_path: Path) -> None:
    _nrf52_state(tmp_path)
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(variant="RP2040")
    ticks = iter(range(0, 1000, 6))
    with (
        patch("serial.Serial"),
        patch(
            "esphome.util.get_serial_ports",
            return_value=[SimpleNamespace(path="/dev/ttyACM0")],
        ),
        patch("time.sleep"),
        patch("time.monotonic", side_effect=lambda: next(ticks)),
    ):
        assert reset_over_usb(_reset_config("RP2040"), "/dev/ttyACM0") is False


def test_reset_over_usb_does_nothing_when_the_build_data_is_missing(
    tmp_path: Path,
) -> None:
    """A reset request must not fail just because the build data cannot be restored."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    with patch("serial.Serial") as mock_serial:
        assert reset_over_usb({}, "/dev/ttyACM0") is False
    mock_serial.assert_not_called()


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------


@pytest.mark.asyncio
async def test_zephyr_add_usb_baud_rate_creates_the_declared_component() -> None:
    from esphome.components.zephyr import zephyr_add_usb_baud_rate

    config = {"usb_baud_rate": "declared_id"}
    with (
        patch("esphome.components.zephyr.zephyr_add_cdc_acm") as mock_cdc,
        patch("esphome.components.zephyr.zephyr_add_prj_conf") as mock_conf,
        patch("esphome.codegen.add_define") as mock_define,
        patch("esphome.codegen.new_Pvariable", return_value="var") as mock_new,
        patch("esphome.codegen.register_component", new=AsyncMock()) as mock_register,
    ):
        assert await zephyr_add_usb_baud_rate(config) == "var"
    mock_define.assert_called_once_with("USE_ZEPHYR_USB_BAUD_RATE")
    mock_cdc.assert_called_once_with(config, 0)
    mock_conf.assert_called_once_with("UART_LINE_CTRL", True)
    mock_new.assert_called_once_with("declared_id")
    mock_register.assert_awaited_once()


async def _run_dfu_to_code(dfu_config: dict) -> MagicMock:
    """Run _dfu_to_code with code generation mocked; returns the new component."""
    var = MagicMock()
    with (
        patch(
            "esphome.components.zephyr.zephyr_add_usb_baud_rate",
            new=AsyncMock(return_value="usb"),
        ),
        patch("esphome.codegen.add_define") as mock_define,
        patch("esphome.codegen.new_Pvariable", return_value=var) as mock_new,
        patch("esphome.codegen.add"),
        patch("esphome.codegen.gpio_pin_expression", new=AsyncMock(return_value="pin")),
        patch("esphome.codegen.register_component", new=AsyncMock()),
    ):
        await nrf52._dfu_to_code(dfu_config)
    mock_define.assert_called_once_with("USE_ZEPHYR_NRF52_DFU")
    mock_new.assert_called_once_with("dfu_id", "usb")
    return var


@pytest.mark.asyncio
async def test_dfu_to_code_sets_the_reset_pin() -> None:
    var = await _run_dfu_to_code({"id": "dfu_id", "reset_pin": "GPIO13"})
    var.set_reset_pin.assert_called_once_with("pin")


@pytest.mark.asyncio
async def test_dfu_to_code_without_a_reset_pin_uses_gpregret() -> None:
    var = await _run_dfu_to_code({"id": "dfu_id"})
    var.set_reset_pin.assert_not_called()


@pytest.mark.asyncio
async def test_bootsel_touch_to_code_subscribes_to_the_usb_component() -> None:
    from esphome.components.zephyr.variants import rpi_pico_family

    advanced = {"bootsel_touch": "touch_id", "usb_baud_rate": "usb_id"}
    with (
        patch(
            "esphome.components.zephyr.zephyr_add_usb_baud_rate",
            new=AsyncMock(return_value="usb"),
        ) as mock_usb,
        patch("esphome.codegen.new_Pvariable") as mock_new,
        patch("esphome.codegen.register_component", new=AsyncMock()) as mock_register,
    ):
        await rpi_pico_family._bootsel_touch_to_code(advanced)
    mock_usb.assert_awaited_once_with(advanced)
    mock_new.assert_called_once_with("touch_id", "usb")
    mock_register.assert_awaited_once()
