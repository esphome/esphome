"""Tests for network component."""

import pytest

from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C6
from esphome.components.network.const import CONF_ENABLE_IPV4
import esphome.config_validation as cv
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

_ESP8266_CORE_DATA = {KEY_FRAMEWORK_VERSION: cv.Version(0, 0, 0)}
_ESP32_CORE_DATA = {KEY_FRAMEWORK_VERSION: cv.Version(0, 0, 0)}
_ESP32_PLATFORM_DATA = {KEY_VARIANT: VARIANT_ESP32C6}


@pytest.mark.parametrize(
    "platform_framework",
    [PlatformFramework.ESP8266_ARDUINO, PlatformFramework.RP2040_ARDUINO],
)
def test_disable_ipv4_non_esp32_rejected(
    platform_framework: PlatformFramework,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(platform_framework, core_data=_ESP8266_CORE_DATA)
    from esphome.components.network import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    config = CONFIG_SCHEMA({CONF_ENABLE_IPV4: False})
    with pytest.raises(cv.Invalid, match="only supported on ESP32"):
        FINAL_VALIDATE_SCHEMA(config)


@pytest.mark.parametrize(
    "denied_component",
    ["esp32_improv", "ethernet", "modem", "mqtt", "udp", "wifi", "wireguard"],
)
def test_disable_ipv4_deny_list_rejected(
    denied_component: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data=_ESP32_CORE_DATA,
        platform_data=_ESP32_PLATFORM_DATA,
        full_config={denied_component: {}},
    )
    from esphome.components.network import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    config = CONFIG_SCHEMA({CONF_ENABLE_IPV4: False})
    with pytest.raises(
        cv.Invalid,
        match=f"Disabling IPv4 is not currently compatible with component {denied_component}",
    ):
        FINAL_VALIDATE_SCHEMA(config)


def test_disable_ipv4_esp32_idf_valid(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data=_ESP32_CORE_DATA,
        platform_data=_ESP32_PLATFORM_DATA,
    )
    from esphome.components.network import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    config = CONFIG_SCHEMA({CONF_ENABLE_IPV4: False})
    FINAL_VALIDATE_SCHEMA(config)  # should not raise
