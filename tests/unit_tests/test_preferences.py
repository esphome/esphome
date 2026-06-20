"""Tests for esphome.preferences storage backend selection."""

import pytest

from esphome import preferences
import esphome.config_validation as cv
from esphome.const import (
    CONF_STORAGE,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
)
from esphome.core import CORE


def _set_platform(platform: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}


def _validate(value: dict):
    return cv.Schema(preferences.storage_schema())(value)


def test_is_in_flash() -> None:
    assert preferences.is_in_flash(preferences.STORAGE_FLASH) is True
    assert preferences.is_in_flash(preferences.STORAGE_RTC) is False


@pytest.mark.parametrize(
    ("platform", "expected"),
    [
        # Defaults preserve each platform's historic behavior.
        (PLATFORM_ESP32, preferences.STORAGE_FLASH),
        (PLATFORM_ESP8266, preferences.STORAGE_RTC),
        (PLATFORM_RP2040, preferences.STORAGE_FLASH),
    ],
)
def test_default_storage_per_platform(platform: str, expected: str) -> None:
    _set_platform(platform)
    assert _validate({})[CONF_STORAGE] == expected


@pytest.mark.parametrize("platform", [PLATFORM_ESP32, PLATFORM_ESP8266])
def test_rtc_allowed_on_supported_platforms(platform: str) -> None:
    _set_platform(platform)
    assert _validate({CONF_STORAGE: "rtc"})[CONF_STORAGE] == preferences.STORAGE_RTC


def test_rtc_rejected_on_unsupported_platform() -> None:
    _set_platform(PLATFORM_RP2040)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _validate({CONF_STORAGE: "rtc"})


def test_flash_allowed_on_unsupported_platform() -> None:
    _set_platform(PLATFORM_RP2040)
    assert _validate({CONF_STORAGE: "flash"})[CONF_STORAGE] == preferences.STORAGE_FLASH
