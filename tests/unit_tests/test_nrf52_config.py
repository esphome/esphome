"""Tests for the nRF52 configuration validation."""

from collections.abc import Generator

import pytest

from esphome.components.nrf52 import (
    _default_self_reset_pin,
    _detect_bootloader,
    _validate_open_dfu_toolchain,
    set_core_data,
)
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT_NRF52_NOSD,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_NRF,
)
from esphome.components.zephyr.const import KEY_PM_STATIC, KEY_ZEPHYR
from esphome.components.zephyr_mcumgr.ota import _validate_bootloader
from esphome.config import path_context
import esphome.config_validation as cv
from esphome.const import CONF_INVERTED, CONF_NUMBER, KEY_CORE, Toolchain
from esphome.core import CORE


def test_detect_bootloader_reports_a_missing_board() -> None:
    """The bootloader check runs before the schema, so it reports the missing key."""
    with pytest.raises(cv.Invalid, match="'board' is a required option"):
        _detect_bootloader({})


@pytest.mark.parametrize(
    ("board", "bootloader"),
    [
        # The dongle's only bootloader is the factory one
        ("nrf52840dongle", BOOTLOADER_NRF),
        # Makerdiary's UF2 bootloader is an Adafruit fork that ships without a SoftDevice
        ("nrf52840_mdk_usb_dongle", BOOTLOADER_ADAFRUIT_NRF52_NOSD),
    ],
)
def test_board_selects_its_factory_bootloader(board: str, bootloader: str) -> None:
    """The factory bootloader is selected without a config line."""
    assert _detect_bootloader({"board": board})["bootloader"] == bootloader


def test_dongle_rejects_foreign_bootloader() -> None:
    with pytest.raises(cv.Invalid, match="nrf52840dongle does not support"):
        _detect_bootloader({"board": "nrf52840dongle", "bootloader": "mcuboot"})


def _dongle_core_data(
    board: str = "nrf52840dongle", bootloader: str = BOOTLOADER_NRF
) -> None:
    CORE.data[KEY_CORE] = {}
    CORE.testing_mode = (
        False  # the real flash, not the larger one faked for grouped tests
    )
    set_core_data({"board": board, "bootloader": bootloader})


@pytest.fixture
def _path_ctx() -> Generator[None]:
    """A pin schema reports its errors against the config path, so one has to be set."""
    token = path_context.set(["nrf52"])
    yield
    path_context.reset(token)


def test_nrf_bootloader_reserves_factory_region() -> None:
    """Only the Open DFU bootloader is registered as a static partition; the board's own
    nrf5_mbr partition takes the first page, so the app lands at 0x1000 where the bootloader loads it."""
    _dongle_core_data()
    registered = {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}
    assert {s.name for s in BOOTLOADER_CONFIG[BOOTLOADER_NRF]} <= set(registered)
    assert "mbr" not in registered
    assert registered["open_bootloader"].address == 0xE0000
    assert registered["open_bootloader"].end_address == 0x100000


def test_softdevice_layout_names_the_mbr_page() -> None:
    """On a board with an nRF5 bootloader the partition manager adds an MBR page of its own
    unless one is declared, which would push the app behind where the SoftDevice jumps to."""
    _dongle_core_data("nrf52840_mdk_usb_dongle", BOOTLOADER_ADAFRUIT_NRF52_SD140_V6)
    registered = {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}
    assert registered["nrf5_mbr"].address == 0x0
    assert registered["nrf5_mbr"].end_address == registered["SoftDevice"].address
    assert registered["SoftDevice"].end_address == 0x26000


def test_adafruit_nosd_reserves_only_the_bootloader() -> None:
    """Without a SoftDevice the app follows the MBR page, as with the Open DFU bootloader."""
    _dongle_core_data("nrf52840_mdk_usb_dongle", BOOTLOADER_ADAFRUIT_NRF52_NOSD)
    registered = {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}
    assert set(registered) == {"Adafruit_nRF52_Bootloader"}
    assert registered["Adafruit_nRF52_Bootloader"].address == 0xF4000
    assert registered["Adafruit_nRF52_Bootloader"].end_address == 0x100000


def test_nrf_bootloader_rejects_mcumgr_ota() -> None:
    """OTA with mcumgr places its slot against a SoftDevice, which this layout has none of."""
    _dongle_core_data()
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
    _dongle_core_data()
    config = _default_self_reset_pin({"board": "nrf52840dongle", "dfu": {}})
    pin = config["dfu"]["reset_pin"]
    assert pin[CONF_NUMBER] == 19
    assert pin[CONF_INVERTED] is True


@pytest.mark.usefixtures("_path_ctx")
def test_dfu_reset_pin_given_by_the_user_is_kept() -> None:
    _dongle_core_data()
    config = _default_self_reset_pin(
        {"board": "nrf52840dongle", "dfu": {"reset_pin": {"number": 5}}}
    )
    assert config["dfu"]["reset_pin"] == {"number": 5}


@pytest.mark.usefixtures("_path_ctx")
def test_other_boards_get_no_reset_pin() -> None:
    _dongle_core_data()
    config = _default_self_reset_pin(
        {"board": "adafruit_itsybitsy_nrf52840", "dfu": {}}
    )
    assert "reset_pin" not in config["dfu"]
