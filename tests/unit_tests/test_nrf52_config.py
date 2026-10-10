"""Tests for the nRF52 configuration validation."""

import pytest

from esphome.components.nrf52 import (
    _detect_bootloader,
    _validate_open_dfu_toolchain,
    set_core_data,
)
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.nrf52.const import BOOTLOADER_NRF
from esphome.components.zephyr.const import KEY_PM_STATIC, KEY_ZEPHYR
from esphome.components.zephyr_mcumgr.ota import _validate_bootloader
import esphome.config_validation as cv
from esphome.const import KEY_CORE, Toolchain
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


def _dongle_core_data() -> None:
    CORE.data[KEY_CORE] = {}
    set_core_data({"board": "nrf52840dongle", "bootloader": BOOTLOADER_NRF})


def test_nrf_bootloader_reserves_factory_region() -> None:
    """The MBR page and the Open DFU bootloader are registered as static partitions around the app."""
    _dongle_core_data()
    registered = {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}
    assert {s.name for s in BOOTLOADER_CONFIG[BOOTLOADER_NRF]} <= set(registered)
    assert registered["mbr"].address == 0x0
    assert registered["open_bootloader"].end_address == 0x100000


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
