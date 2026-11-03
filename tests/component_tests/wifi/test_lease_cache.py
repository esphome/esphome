"""Tests for the lease_cache storage validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_VARIANT
from esphome.components.esp32.const import VARIANT_ESP32, VARIANT_ESP32C2
from esphome.components.wifi import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _validate(lease_cache: dict) -> dict:
    return CONFIG_SCHEMA(
        {
            "ssid": "net",
            "password": "secret12",
            "use_address": "test.local",
            "lease_cache": lease_cache,
        }
    )


@pytest.mark.parametrize(
    "framework", [PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO]
)
def test_storage_defaults_to_rtc(
    set_core_config: SetCoreConfigCallable, framework: PlatformFramework
) -> None:
    """Both ESP32 frameworks share the backend the cache lives in."""
    set_core_config(framework, platform_data={KEY_VARIANT: VARIANT_ESP32})
    config = _validate({"max_age": "12h"})
    assert config["lease_cache"]["storage"] == "rtc"


def test_lease_cache_requires_esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="only available on"):
        _validate({"max_age": "12h"})


def test_no_rtc_variant_requires_flash(set_core_config: SetCoreConfigCallable) -> None:
    """A variant without RTC memory must say storage: flash; the error points at that key."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C2}
    )
    with pytest.raises(cv.Invalid, match="no RTC memory") as exc:
        _validate({"max_age": "12h"})
    assert exc.value.path == ["lease_cache", "storage"]
    config = _validate({"storage": "flash"})
    assert config["lease_cache"]["storage"] == "flash"
