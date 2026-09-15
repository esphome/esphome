"""Tests for esphome.components.nrf52 bootloader/board contract.

The nrf52840dongle (PCA10059) ships with Nordic's Open DFU Bootloader
factory-flashed at 0xE0000-0x100000. The build must reserve that region
instead of staging anything there, and mcumgr OTA must be rejected for
`bootloader: nrf` — the Open BL has no secondary slot to download into,
so an accepted-but-unstaged OTA build would brick on update.
"""

import pytest

from esphome.components.nrf52 import _detect_bootloader, set_core_data
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.nrf52.const import BOOTLOADER_NRF
from esphome.components.zephyr.const import KEY_PM_STATIC, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE

# addresses from the PCA10059 memory map / Nordic Open DFU Bootloader
_OPEN_BOOTLOADER_ADDRESS = 0xE0000
_OPEN_BOOTLOADER_SIZE = 0x20000
_SETTINGS_ADDRESS = 0xD8000


def _detect(board: str, bootloader: str | None = None):
    config: dict = {"board": board}
    if bootloader is not None:
        config["bootloader"] = bootloader
    return _detect_bootloader(config)


def test_dongle_defaults_to_nrf_bootloader():
    """board: nrf52840dongle with no bootloader line selects bootloader: nrf."""
    config = _detect("nrf52840dongle")
    assert config["bootloader"] == BOOTLOADER_NRF


def test_dongle_rejects_foreign_bootloader():
    """An explicit unsupported bootloader on the dongle fails validation."""
    with pytest.raises(cv.Invalid, match="nrf52840dongle does not support"):
        _detect("nrf52840dongle", "mcuboot")


def test_nrf_bootloader_reserves_open_bootloader_region():
    """BOOTLOADER_CONFIG for nrf stages the Open BL region as reserved."""
    sections = BOOTLOADER_CONFIG[BOOTLOADER_NRF]
    by_name = {s.name: s for s in sections}
    assert set(by_name) == {"settings_storage", "open_bootloader"}
    bootloader = by_name["open_bootloader"]
    assert bootloader.address == _OPEN_BOOTLOADER_ADDRESS
    assert bootloader.size == _OPEN_BOOTLOADER_SIZE
    assert by_name["settings_storage"].address == _SETTINGS_ADDRESS
    # the reserved region must not overlap settings storage
    assert (
        bootloader.address
        >= by_name["settings_storage"].address + by_name["settings_storage"].size
    )


def test_nrf_bootloader_sections_are_unique_across_configs():
    """Section names emitted for nrf must not collide with other bootloaders'."""
    for name, sections in BOOTLOADER_CONFIG.items():
        if name == BOOTLOADER_NRF:
            continue
        names = {s.name for s in sections}
        assert not (names & {"open_bootloader"}), name


def test_dongle_pm_static_sections_land_in_zephyr_data():
    """set_core_data path: nrf bootloader sections get registered as pm_static."""
    CORE.data = {KEY_CORE: {KEY_TARGET_PLATFORM: None}}  # type: ignore[assignment]
    config = {
        "board": "nrf52840dongle",
        "bootloader": BOOTLOADER_NRF,
    }
    set_core_data(config)
    zephyr = CORE.data.get(KEY_ZEPHYR)
    assert zephyr is not None
    registered = [s.name for s in zephyr[KEY_PM_STATIC]]
    assert "open_bootloader" in registered
    assert "settings_storage" in registered
