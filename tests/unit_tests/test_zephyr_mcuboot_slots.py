"""Tests for the MCUboot image slot helpers."""

import pytest

from esphome.components.zephyr import (
    Section,
    zephyr_add_pm_static,
    zephyr_mcuboot_slot_size,
    zephyr_set_core_data,
)
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER
from esphome.const import CONF_BOARD
from esphome.core import EsphomeError


@pytest.fixture(autouse=True)
def zephyr_core() -> None:
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})


def test_slots_end_at_the_lowest_section_above_them() -> None:
    zephyr_add_pm_static(
        [
            Section("mcuboot", 0x0, 0x10000, "flash_primary"),
            Section("coredump_partition", 0xEE000, 0x8000, "flash_primary"),
            Section("settings_storage", 0xF6000, 0xA000, "flash_primary"),
        ]
    )

    assert zephyr_mcuboot_slot_size(0x10000) == 0x6F000


def test_nothing_reserved_above_the_slots_is_reported() -> None:
    zephyr_add_pm_static([Section("mcuboot", 0x0, 0x10000, "flash_primary")])

    with pytest.raises(
        EsphomeError, match="No flash section is reserved above 0x10000"
    ):
        zephyr_mcuboot_slot_size(0x10000)


def test_a_section_just_above_slot0_is_named() -> None:
    zephyr_add_pm_static([Section("too_low", 0x11000, 0x1000, "flash_primary")])

    with pytest.raises(EsphomeError, match="'too_low' at 0x11000 leaves"):
        zephyr_mcuboot_slot_size(0x10000)


def test_a_section_straddling_slot0_is_named() -> None:
    zephyr_add_pm_static(
        [
            Section("mcuboot", 0x0, 0x12000, "flash_primary"),
            Section("settings_storage", 0xF6000, 0xA000, "flash_primary"),
        ]
    )

    with pytest.raises(
        EsphomeError, match="'mcuboot' overlaps the MCUboot image slots"
    ):
        zephyr_mcuboot_slot_size(0x10000)
