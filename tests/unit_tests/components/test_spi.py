"""Unit tests for esphome.components.spi (Zephyr quad-mode schema gate)."""

from __future__ import annotations

import pytest

from esphome.components.spi import _quad_platform_validator
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
