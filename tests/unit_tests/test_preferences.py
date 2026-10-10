"""Tests for esphome.preferences storage backend selection."""

import pytest

from esphome import preferences
from esphome.components.esp32 import KEY_ESP32
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C61,
)
from esphome.components.libretiny.const import (
    FAMILY_BK7231N,
    FAMILY_BK7231T,
    FAMILY_BK7251,
    FAMILY_RTL8710B,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_STORAGE,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    KEY_VARIANT,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE


def _set_platform(platform: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}


def _set_esp32(variant: str) -> None:
    _set_platform(PLATFORM_ESP32)
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}


def _validate(value: dict):
    return cv.Schema(preferences.storage_schema())(value)


def _define_names() -> set[str]:
    return {define.name for define in CORE.defines}


def test_is_in_flash() -> None:
    _set_platform(PLATFORM_ESP8266)
    assert preferences.is_in_flash(preferences.STORAGE_FLASH) is True
    assert preferences.is_in_flash(preferences.STORAGE_RTC) is False
    # The RTC storage define is ESP32-specific.
    assert "USE_ESP32_RTC_PREFERENCES" not in _define_names()


def test_is_in_flash_esp32_rtc_emits_define() -> None:
    _set_esp32(VARIANT_ESP32)
    assert preferences.is_in_flash(preferences.STORAGE_FLASH) is True
    assert "USE_ESP32_RTC_PREFERENCES" not in _define_names()
    assert preferences.is_in_flash(preferences.STORAGE_RTC) is False
    assert "USE_ESP32_RTC_PREFERENCES" in _define_names()


def test_request_rtc_storage_esp32_only() -> None:
    _set_platform(PLATFORM_ESP8266)
    preferences.request_rtc_storage()
    # ESP8266 always has its RTC backend; no define is needed or emitted.
    assert "USE_ESP32_RTC_PREFERENCES" not in _define_names()


def test_request_rtc_storage_esp32_emits_define() -> None:
    _set_esp32(VARIANT_ESP32)
    preferences.request_rtc_storage()
    assert "USE_ESP32_RTC_PREFERENCES" in _define_names()


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32C3])
def test_validate_rtc_storage_accepted(variant: str) -> None:
    _set_esp32(variant)
    assert preferences.validate_rtc_storage(True) is True
    assert preferences.validate_rtc_storage(False) is False


def test_validate_rtc_storage_esp8266() -> None:
    _set_platform(PLATFORM_ESP8266)
    # Tolerated no-op: the ESP8266 backend always has RTC storage.
    assert preferences.validate_rtc_storage(True) is True
    # But it cannot be disabled, so an explicit false is an error.
    with pytest.raises(cv.Invalid, match="always enabled on ESP8266"):
        preferences.validate_rtc_storage(False)


@pytest.mark.parametrize("variant", [VARIANT_ESP32C2, VARIANT_ESP32C61])
def test_validate_rtc_storage_rejected_without_rtc_memory(variant: str) -> None:
    _set_esp32(variant)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        preferences.validate_rtc_storage(True)
    # Disabling it is always fine.
    assert preferences.validate_rtc_storage(False) is False


def test_validate_rtc_storage_rejected_on_unsupported_platform() -> None:
    _set_platform(PLATFORM_RP2040)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        preferences.validate_rtc_storage(True)


@pytest.mark.parametrize(
    ("platform", "expected"),
    [
        # Defaults preserve each platform's historic behavior.
        (PLATFORM_ESP8266, preferences.STORAGE_RTC),
        (PLATFORM_RP2040, preferences.STORAGE_FLASH),
    ],
)
def test_default_storage_per_platform(platform: str, expected: str) -> None:
    _set_platform(platform)
    assert _validate({})[CONF_STORAGE] == expected


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32C2])
def test_default_storage_esp32_is_flash(variant: str) -> None:
    # ESP32 defaults to flash on every variant, including those without RTC memory.
    _set_esp32(variant)
    assert _validate({})[CONF_STORAGE] == preferences.STORAGE_FLASH


def test_rtc_allowed_on_esp8266() -> None:
    _set_platform(PLATFORM_ESP8266)
    assert _validate({CONF_STORAGE: "rtc"})[CONF_STORAGE] == preferences.STORAGE_RTC


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32C3])
def test_rtc_allowed_on_esp32_with_rtc_memory(variant: str) -> None:
    _set_esp32(variant)
    assert _validate({CONF_STORAGE: "rtc"})[CONF_STORAGE] == preferences.STORAGE_RTC


@pytest.mark.parametrize("variant", [VARIANT_ESP32C2, VARIANT_ESP32C61])
def test_rtc_rejected_on_esp32_without_rtc_memory(variant: str) -> None:
    _set_esp32(variant)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _validate({CONF_STORAGE: "rtc"})


def test_rtc_rejected_on_unsupported_platform() -> None:
    _set_platform(PLATFORM_RP2040)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _validate({CONF_STORAGE: "rtc"})


def test_flash_allowed_on_unsupported_platform() -> None:
    _set_platform(PLATFORM_RP2040)
    assert _validate({CONF_STORAGE: "flash"})[CONF_STORAGE] == preferences.STORAGE_FLASH


@pytest.mark.parametrize("variant", [VARIANT_ESP32, VARIANT_ESP32C3])
def test_rtc_supported_on_esp32_with_rtc_memory(variant: str) -> None:
    _set_esp32(variant)
    assert preferences.rtc_supported() is True


@pytest.mark.parametrize("variant", [VARIANT_ESP32C2, VARIANT_ESP32C61])
def test_rtc_supported_false_without_rtc_memory(variant: str) -> None:
    _set_esp32(variant)
    assert preferences.rtc_supported() is False


@pytest.mark.parametrize(
    ("platform", "expected"),
    [(PLATFORM_ESP8266, True), (PLATFORM_RP2040, False)],
)
def test_rtc_supported_per_platform(platform: str, expected: bool) -> None:
    _set_platform(platform)
    assert preferences.rtc_supported() is expected


def test_validate_storage_accepts_flash_everywhere() -> None:
    _set_platform(PLATFORM_RP2040)
    assert preferences.validate_storage("flash") == preferences.STORAGE_FLASH
    assert preferences.validate_storage("FLASH") == preferences.STORAGE_FLASH


def test_validate_storage_accepts_rtc_where_supported() -> None:
    _set_esp32(VARIANT_ESP32)
    assert preferences.validate_storage("rtc") == preferences.STORAGE_RTC


def test_validate_storage_rejects_rtc_without_rtc_memory() -> None:
    _set_esp32(VARIANT_ESP32C2)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        preferences.validate_storage("rtc")


def test_validate_storage_rejects_unknown_value() -> None:
    _set_esp32(VARIANT_ESP32)
    with pytest.raises(cv.Invalid):
        preferences.validate_storage("nvs")


def _set_libretiny(platform: str, family: str) -> None:
    _set_platform(platform)
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family}


def test_rtc_allowed_on_bk7231t() -> None:
    # Emulated with flash plus the reset reason, which was checked on hardware.
    _set_libretiny(PLATFORM_BK72XX, FAMILY_BK7231T)
    assert _validate({CONF_STORAGE: "rtc"})[CONF_STORAGE] == preferences.STORAGE_RTC


@pytest.mark.parametrize(
    ("platform", "family"),
    [
        (PLATFORM_BK72XX, FAMILY_BK7231N),
        (PLATFORM_BK72XX, FAMILY_BK7251),
        (PLATFORM_RTL87XX, FAMILY_RTL8710B),
    ],
)
def test_rtc_rejected_on_other_libretiny_families(platform: str, family: str) -> None:
    _set_libretiny(platform, family)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _validate({CONF_STORAGE: "rtc"})


def test_default_storage_bk7231t_is_flash() -> None:
    _set_libretiny(PLATFORM_BK72XX, FAMILY_BK7231T)
    assert _validate({})[CONF_STORAGE] == preferences.STORAGE_FLASH


def test_request_rtc_storage_bk7231t_emits_define() -> None:
    _set_libretiny(PLATFORM_BK72XX, FAMILY_BK7231T)
    preferences.request_rtc_storage()
    assert "USE_LIBRETINY_RTC_PREFERENCES" in _define_names()


def test_request_rtc_storage_other_libretiny_family_is_a_no_op() -> None:
    _set_libretiny(PLATFORM_BK72XX, FAMILY_BK7231N)
    preferences.request_rtc_storage()
    assert "USE_LIBRETINY_RTC_PREFERENCES" not in _define_names()
