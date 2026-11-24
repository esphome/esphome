"""Tests for the nRF52 configuration validation."""

from collections.abc import Generator
import itertools
import logging

import pytest

from esphome.components.nrf52 import (
    _default_dcdc,
    _default_self_reset_pin,
    _detect_bootloader,
    _validate_mcumgr,
    _validate_open_dfu_toolchain,
    _warn_without_antenna,
    set_core_data,
)
from esphome.components.nrf52.boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG
from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V2,
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V6,
    BOOTLOADER_NRF,
    VARIANT_NRF54LM20,
    VARIANT_NRF52832,
)
from esphome.components.nrf52.framework import get_nrf52_board, get_nrf52_variant
from esphome.components.zephyr import Section
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_ZEPHYR,
)
from esphome.components.zephyr_mcumgr.ota import _validate_bootloader
from esphome.config import path_context
import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_NUMBER, KEY_CORE, Toolchain
from esphome.core import CORE


def test_detect_bootloader_reports_a_missing_board() -> None:
    """The bootloader check runs before the schema, so it reports the missing key."""
    with pytest.raises(cv.Invalid, match="'board' is a required option"):
        _detect_bootloader({})


def test_dongle_defaults_to_nrf_bootloader() -> None:
    """The dongle's only bootloader is the factory one, so it is selected without a config line."""
    config = _detect_bootloader({"board": "nrf52840dongle"})
    assert config["bootloader"] == BOOTLOADER_NRF


def test_dongle_rejects_foreign_bootloader() -> None:
    with pytest.raises(cv.Invalid, match="nrf52840dongle does not support"):
        _detect_bootloader({"board": "nrf52840dongle", "bootloader": "mcuboot"})


def _core_data(
    board: str = "nrf52840dongle", bootloader: str = BOOTLOADER_NRF
) -> dict[str, Section]:
    CORE.data[KEY_CORE] = {}
    CORE.testing_mode = (
        False  # the real flash, not the larger one faked for grouped tests
    )
    set_core_data({"board": board, "bootloader": bootloader})
    return {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}


@pytest.fixture
def _path_ctx() -> Generator[None]:
    """A pin schema reports its errors against the config path, so one has to be set."""
    token = path_context.set(["nrf52"])
    yield
    path_context.reset(token)


def test_nrf_bootloader_reserves_factory_region() -> None:
    """Only the Open DFU bootloader is registered as a static partition; the board's own
    nrf5_mbr partition takes the first page, so the app lands at 0x1000 where the bootloader loads it."""
    registered = _core_data()
    assert {s.name for s in BOOTLOADER_CONFIG[BOOTLOADER_NRF]} <= set(registered)
    assert "mbr" not in registered
    assert registered["open_bootloader"].address == 0xE0000
    assert registered["open_bootloader"].end_address == 0x100000


def test_nrf_bootloader_rejects_mcumgr_ota() -> None:
    """OTA with mcumgr places its slot against a SoftDevice, which this layout has none of."""
    _core_data()
    with pytest.raises(cv.Invalid, match="does not support OTA"):
        _validate_bootloader({})


def test_nrf_bootloader_accepts_the_sdk_nrf_toolchain() -> None:
    CORE.toolchain = Toolchain.SDK_NRF
    _validate_open_dfu_toolchain({"bootloader": BOOTLOADER_NRF})


def test_nrf_bootloader_rejects_the_platformio_toolchain() -> None:
    """The deprecated PlatformIO upload path assumes a touch-reset bootloader."""
    CORE.toolchain = Toolchain.PLATFORMIO
    with pytest.raises(cv.Invalid, match="sdk-nrf"):
        _validate_open_dfu_toolchain({"bootloader": BOOTLOADER_NRF})


@pytest.mark.usefixtures("_path_ctx")
def test_dongle_dfu_resets_itself_through_the_pin_wired_to_reset() -> None:
    """The factory bootloader ignores the touch's register, so P0.19 pulls RESET for it."""
    _core_data()
    config = _default_self_reset_pin({"board": "nrf52840dongle", "dfu": {}})
    pin = config["dfu"]["reset_pin"]
    assert pin[CONF_NUMBER] == 19
    assert pin[CONF_INVERTED] is True


@pytest.mark.usefixtures("_path_ctx")
def test_dfu_reset_pin_given_by_the_user_is_kept() -> None:
    _core_data()
    config = _default_self_reset_pin(
        {"board": "nrf52840dongle", "dfu": {"reset_pin": {"number": 5}}}
    )
    assert config["dfu"]["reset_pin"] == {"number": 5}


@pytest.mark.usefixtures("_path_ctx")
def test_other_boards_get_no_reset_pin() -> None:
    _core_data()
    config = _default_self_reset_pin(
        {"board": "adafruit_itsybitsy_nrf52840", "dfu": {}}
    )
    assert "reset_pin" not in config["dfu"]


FEATHER_NRF52832 = "adafruit_feather_nrf52832"


@pytest.mark.parametrize(
    ("given", "selected"),
    [
        ({}, BOOTLOADER_ADAFRUIT_NRF52_SD132_V6),
        (
            {"bootloader": BOOTLOADER_ADAFRUIT_NRF52_SD132_V2},
            BOOTLOADER_ADAFRUIT_NRF52_SD132_V2,
        ),
    ],
)
def test_feather_nrf52832_bootloaders(given: dict[str, str], selected: str) -> None:
    """The current bootloader is the default; the original one can be selected."""
    config = _detect_bootloader({"board": FEATHER_NRF52832, **given})
    assert config["bootloader"] == selected


def test_feather_nrf52832_rejects_an_nrf52840_bootloader() -> None:
    with pytest.raises(cv.Invalid, match="adafruit_feather_nrf52832 does not support"):
        _detect_bootloader(
            {"board": FEATHER_NRF52832, "bootloader": "adafruit_nrf52_sd140_v6"}
        )


def test_feather_nrf52832_builds_on_the_nrf52_dk() -> None:
    """Zephyr has no Feather nRF52832 board; the nRF52 DK has its console UART on the same pins."""
    _core_data(FEATHER_NRF52832, BOOTLOADER_ADAFRUIT_NRF52_SD132_V6)
    assert CORE.data[KEY_ZEPHYR][KEY_BOARD] == "nrf52dk/nrf52832"
    assert get_nrf52_variant() == VARIANT_NRF52832


def test_feather_nrf52832_current_bootloader_layout() -> None:
    """S132 6.1.1 ends at 0x26000, and the bootloader fills 0x74000 to the end of the 512 KB flash."""
    registered = _core_data(FEATHER_NRF52832, BOOTLOADER_ADAFRUIT_NRF52_SD132_V6)
    assert registered["SoftDevice"].end_address == 0x26000
    assert registered["Adafruit_nRF52_Bootloader"].address == 0x74000
    assert registered["Adafruit_nRF52_Bootloader"].end_address == 0x80000


def test_feather_nrf52832_original_bootloader_layout() -> None:
    """The dual bank bootloader takes at most 0x28800 bytes at 0x1C000, after S132 2.0.1."""
    registered = _core_data(FEATHER_NRF52832, BOOTLOADER_ADAFRUIT_NRF52_SD132_V2)
    assert registered["SoftDevice"].end_address == 0x1C000
    # The sections above the app are contiguous, so the partition manager sees one gap for it
    above = sorted(
        (s for s in registered.values() if s.address > 0), key=lambda s: s.address
    )
    assert all(a.end_address == b.address for a, b in itertools.pairwise(above))
    assert above[-1].end_address == 0x80000


def test_dfu_component_needs_usb() -> None:
    """The touch that enters DFU is a baud rate change on a USB CDC port."""
    _core_data(FEATHER_NRF52832, BOOTLOADER_ADAFRUIT_NRF52_SD132_V6)
    with pytest.raises(cv.Invalid, match="'dfu' needs USB"):
        _validate_mcumgr({})


@pytest.mark.parametrize(
    ("given", "dcdc"),
    [
        ({"board": FEATHER_NRF52832}, False),
        ({"board": FEATHER_NRF52832, "dcdc": True}, True),
        ({"board": "xiao_ble"}, None),
    ],
)
def test_board_dcdc_default(given: dict[str, str | bool], dcdc: bool | None) -> None:
    """The Feather defaults to LDO; a user setting wins and other boards get nothing."""
    assert _default_dcdc(given).get("dcdc") is dcdc


def test_xiao_nrf54lm20a_builds_on_the_nrf54lm20_dk() -> None:
    """sdk-nrf 3.4.1 has no XIAO nRF54LM20A board; it builds on the nRF54LM20 DK with MCUboot."""
    assert _detect_bootloader({"board": "xiao_nrf54lm20a"})["bootloader"] == (
        BOOTLOADER_MCUBOOT
    )
    _core_data("xiao_nrf54lm20a", BOOTLOADER_MCUBOOT)
    assert CORE.data[KEY_ZEPHYR][KEY_BOARD] == "nrf54lm20dk/nrf54lm20a/cpuapp"
    assert get_nrf52_variant() == VARIANT_NRF54LM20
    # The logger default follows the board, not the DK it builds on
    assert get_nrf52_board() == "xiao_nrf54lm20a"


def test_xiao_nrf54lm20a_console_uart_on_the_debugger_pins() -> None:
    """The console UART uses the pins of the on-board debugger: TX P1.11, RX P1.10."""
    overlay = BOARDS_ZEPHYR["xiao_nrf54lm20a"][KEY_OVERLAY]
    assert overlay.count("NRF_PSEL(UART_TX, 1, 11)") == 2
    assert overlay.count("NRF_PSEL(UART_RX, 1, 10)") == 2
    assert "RTS" not in overlay
    assert "CTS" not in overlay


@pytest.mark.parametrize(
    ("board", "warns"),
    [("xiao_nrf54lm20a", True), ("nrf54lm20dk/nrf54lm20a/cpuapp", False)],
)
def test_warns_about_a_board_without_antenna(
    caplog: pytest.LogCaptureFixture, board: str, warns: bool
) -> None:
    """A board with only an antenna connector needs an external antenna for any radio."""
    with caplog.at_level(logging.WARNING):
        _warn_without_antenna({"board": board})
    assert ("has no antenna of its own" in caplog.text) is warns
