"""Tests for the nRF52 configuration validation."""

import pytest

from esphome.components.nrf52 import _detect_bootloader, set_core_data
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.nrf52.const import BOOTLOADER_NRF
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_PM_STATIC, KEY_ZEPHYR
from esphome.components.zephyr_mcumgr.ota import _validate_bootloader
import esphome.config_validation as cv
from esphome.const import KEY_CORE
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


def test_nrf_bootloader_reserves_factory_region() -> None:
    """The factory Open DFU bootloader stays at the top of flash and nothing else is placed on it."""
    CORE.data[KEY_CORE] = {}
    set_core_data({"board": "nrf52840dongle", "bootloader": BOOTLOADER_NRF})
    registered = {s.name: s for s in CORE.data[KEY_ZEPHYR][KEY_PM_STATIC]}
    assert {s.name for s in BOOTLOADER_CONFIG[BOOTLOADER_NRF]} <= set(registered)
    bootloader = registered["open_bootloader"]
    assert bootloader.end_address == 0x100000
    assert registered["settings_storage"].end_address <= bootloader.address
    # The MBR page stays reserved so the application is linked from 0x1000, where the bootloader chains to
    assert registered["mbr"].address == 0x0
    assert registered["mbr"].end_address == 0x1000


def test_nrf_bootloader_rejects_mcumgr_ota() -> None:
    """The factory Open DFU bootloader has no second image slot, so OTA is refused at validation."""
    CORE.data[KEY_CORE] = {}
    set_core_data({"board": "nrf52840dongle", "bootloader": BOOTLOADER_NRF})
    assert CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] == BOOTLOADER_NRF
    with pytest.raises(cv.Invalid, match="does not support OTA"):
        _validate_bootloader({})
