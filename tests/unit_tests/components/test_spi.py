"""Unit tests for esphome.components.spi (Zephyr quad-mode schema gate, interface selection)."""

from __future__ import annotations

import pytest

from esphome.components.esp32 import KEY_ESP32
from esphome.components.spi import _quad_platform_validator, one_of_interface_validator
from esphome.components.zephyr.const import KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_RP2,
    PLATFORM_ZEPHYR,
)
from esphome.core import CORE


def test_quad_platform_validator_accepts_esp32_arduino_idf() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_accepts_zephyr_esp32_family_variant() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "ESP32C6"}
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_rejects_zephyr_non_esp32_variant() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "STM32F4"}
    with pytest.raises(cv.Invalid, match="Quad SPI is not available"):
        _quad_platform_validator("x")


def test_quad_platform_validator_rejects_non_esp32_non_zephyr_platform() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_RP2}
    with pytest.raises(cv.Invalid):
        _quad_platform_validator("x")


# ---------------------------------------------------------------------------
# one_of_interface_validator -- Zephyr's named-bus 'interface: spi2' selection
# (item 47: replaces the old dts_node_override key with the same shape ESP32/
# RP2040 already use). Zephyr's real bus list isn't known until to_code() runs
# fetch_board_dts(), so unlike ESP32/RP2040 a named value can't be validated
# against a real list at schema time -- it's accepted and checked for real later
# by resolve_zephyr_bus()/zephyr_setup_spi_pinctrl().
# ---------------------------------------------------------------------------


def test_one_of_interface_validator_zephyr_accepts_generic_values() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    validator = one_of_interface_validator(["software", "hardware", "any"])
    assert validator("Any") == "any"
    assert validator("software") == "software"


def test_one_of_interface_validator_zephyr_passes_through_named_bus() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    validator = one_of_interface_validator(["hardware"])
    assert validator("spi2") == "spi2"


def test_one_of_interface_validator_esp32_rejects_unknown_value() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    CORE.data[KEY_ESP32] = {}
    validator = one_of_interface_validator(["software", "hardware", "any"])
    with pytest.raises(cv.Invalid):
        validator("spi9")
