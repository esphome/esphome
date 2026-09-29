"""Unit tests for esphome.components.nrf52 (core helpers)."""

from __future__ import annotations

import pytest

from esphome.components.nrf52 import _detect_bootloader
from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
)
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_BOARD


def test_detect_bootloader_picks_first_known_bootloader_for_bare_board() -> None:
    config = _detect_bootloader({CONF_BOARD: "xiao_ble"})
    assert config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT_NRF52_SD140_V7


def test_detect_bootloader_defaults_to_mcuboot_for_unknown_board() -> None:
    config = _detect_bootloader({CONF_BOARD: "some_unknown_board"})
    assert config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT


def test_detect_bootloader_accepts_explicit_supported_choice() -> None:
    config = _detect_bootloader(
        {CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT}
    )
    assert config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT


def test_detect_bootloader_rejects_unsupported_explicit_choice() -> None:
    with pytest.raises(cv.Invalid, match="does not support"):
        _detect_bootloader({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})
