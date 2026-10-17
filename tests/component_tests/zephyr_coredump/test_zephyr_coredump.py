"""Tests for the placement of the zephyr_coredump flash partition."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.nrf52 import set_core_data
from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_OVERLAY, KEY_PM_STATIC
from esphome.components.zephyr_coredump import (
    DUMP_THREADS_MIN_VERSION,
    PARTITION_SIZE_MIN,
    PARTITION_SIZE_THREADS,
    _partition_address,
    _partition_size,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


def _core_data(
    board: str,
    bootloader: str,
    testing_mode: bool = False,
    version: cv.Version = DUMP_THREADS_MIN_VERSION,
) -> None:
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: version}
    CORE.testing_mode = testing_mode
    set_core_data({"board": board, "bootloader": bootloader})


@pytest.mark.parametrize(
    ("board", "bootloader", "testing_mode", "expected"),
    [
        # Below the Adafruit bootloader at 0xF4000
        ("adafruit_itsybitsy_nrf52840", "adafruit_nrf52_sd140_v6", False, 0xEC000),
        # Below the factory Open DFU bootloader at 0xE0000
        ("nrf52840dongle", "nrf", False, 0xD8000),
        # Nothing reserved: the end of the 1 MB flash
        ("adafruit_feather_nrf52840", "mcuboot", False, 0xF8000),
        # Testing mode moves the bootloader to the end of the faked 4 MB flash
        ("adafruit_itsybitsy_nrf52840", "adafruit_nrf52_sd140_v6", True, 0x3EC000),
        ("adafruit_feather_nrf52840", "mcuboot", True, 0x3F8000),
    ],
)
def test_partition_address(
    board: str, bootloader: str, testing_mode: bool, expected: int
) -> None:
    _core_data(board, bootloader, testing_mode)
    assert _partition_address(_partition_size()) == expected


def test_partition_size_by_dump_mode() -> None:
    _core_data("adafruit_feather_nrf52840", "mcuboot")
    assert _partition_size() == PARTITION_SIZE_THREADS
    _core_data("adafruit_feather_nrf52840", "mcuboot", version=cv.Version(2, 6, 1))
    assert _partition_size() == PARTITION_SIZE_MIN


def test_unknown_flash_size_rejected() -> None:
    _core_data("bbc_microbit", "mcuboot")
    with pytest.raises(cv.Invalid, match="only nRF52840 boards are supported"):
        _partition_address(_partition_size())


def test_mcumgr_slot_ends_below_partition(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The OTA image slot1 must end at or before the coredump partition."""
    generate_main(component_config_path("mcumgr_adafruit.yaml"))
    sections = {s.name: s for s in zephyr_data()[KEY_PM_STATIC]}
    coredump = sections["coredump_partition"]
    assert coredump.address == 0xEC000
    match = re.search(
        r"slot1_partition: partition@\w+ \{\s*reg = <(0x\w+) (0x\w+)>",
        zephyr_data()[KEY_OVERLAY][""],
    )
    assert match is not None
    start, size = (int(v, 16) for v in match.groups())
    assert start + size <= coredump.address
