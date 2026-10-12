"""Platform restrictions for minimum Wi-Fi authentication modes."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_VARIANT
from esphome.components.esp32.const import VARIANT_ESP32
from esphome.components.wifi import CONF_MIN_AUTH_MODE, CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _validate(mode: str | None = None) -> dict:
    config = {"ssid": "net", "password": "secret12", "use_address": "test.local"}
    if mode is not None:
        config[CONF_MIN_AUTH_MODE] = mode
    return CONFIG_SCHEMA(config)


@pytest.mark.parametrize("mode", ["WPA3", "wpa3"])
def test_esp8266_rejects_wpa3(
    set_core_config: SetCoreConfigCallable, mode: str
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="ESP8266 does not support WPA3") as exc:
        _validate(mode)
    assert exc.value.path == [CONF_MIN_AUTH_MODE]


@pytest.mark.parametrize(
    ("platform", "mode"),
    [
        (PlatformFramework.ESP8266_ARDUINO, "WPA"),
        (PlatformFramework.ESP8266_ARDUINO, "WPA2"),
        (PlatformFramework.ESP32_IDF, "WPA"),
        (PlatformFramework.ESP32_IDF, "WPA2"),
        (PlatformFramework.ESP32_IDF, "WPA3"),
        (PlatformFramework.ESP32_IDF, "wpa3"),
        (PlatformFramework.ESP32_ARDUINO, "WPA"),
        (PlatformFramework.ESP32_ARDUINO, "WPA2"),
        (PlatformFramework.ESP32_ARDUINO, "WPA3"),
    ],
)
def test_supported_modes(
    set_core_config: SetCoreConfigCallable, platform: PlatformFramework, mode: str
) -> None:
    set_core_config(
        platform,
        platform_data=(
            {KEY_VARIANT: VARIANT_ESP32}
            if platform
            in (PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO)
            else None
        ),
    )
    assert _validate(mode)[CONF_MIN_AUTH_MODE] == mode.upper()


@pytest.mark.parametrize(
    "platform",
    [
        PlatformFramework.ESP8266_ARDUINO,
        PlatformFramework.ESP32_IDF,
        PlatformFramework.ESP32_ARDUINO,
    ],
)
def test_default_remains_wpa2(
    set_core_config: SetCoreConfigCallable, platform: PlatformFramework
) -> None:
    set_core_config(
        platform,
        platform_data=(
            {KEY_VARIANT: VARIANT_ESP32}
            if platform
            in (PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO)
            else None
        ),
    )
    assert _validate()[CONF_MIN_AUTH_MODE] == "WPA2"
