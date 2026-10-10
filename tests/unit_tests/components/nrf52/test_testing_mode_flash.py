"""Testing mode fakes a larger flash so grouped component tests link."""

from esphome.components import nrf52
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT_NRF52_SD140_V7
from esphome.components.zephyr import Section


def test_testing_mode_sections_move_only_the_bootloader() -> None:
    """The bootloader pinned to the end of flash follows the faked end; the
    SoftDevice at the start stays."""
    sections = nrf52._testing_mode_sections(
        BOOTLOADER_CONFIG[BOOTLOADER_ADAFRUIT_NRF52_SD140_V7]
    )

    assert [(s.name, s.address, s.size) for s in sections] == [
        ("SoftDevice", 0x0, 0x27000),
        (
            "Adafruit_nRF52_Bootloader",
            nrf52.TESTING_FLASH_SIZE - 0xC000,
            0xC000,
        ),
    ]
    assert sections[1].end_address == nrf52.TESTING_FLASH_SIZE


def test_testing_mode_sections_leave_unpinned_sections() -> None:
    section = Section("other", 0x50000, 0x1000, "flash_primary")

    assert nrf52._testing_mode_sections([section]) == [section]
