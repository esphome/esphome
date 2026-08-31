"""Tests for the api outgoing_connection option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import CONFIG_SCHEMA
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

KEY = "bOFFzzvfpg5DB94DuBGLXD/hMnhpDKgP9UQyBulwWVU="
ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}


def _api_config(outgoing: ConfigType, *, encryption: bool = True) -> ConfigType:
    config: ConfigType = {"outgoing_connection": outgoing}
    if encryption:
        config["encryption"] = {"key": KEY}
    return config


def test_outgoing_connection_generates_defines(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A valid config emits the compile-time defines with defaults applied."""
    generate_main("tests/component_tests/api/test_outgoing_connection.yaml")

    defines = {define.name: define.value for define in CORE.defines}
    assert "USE_API_OUTGOING_CONNECTION" in defines
    assert str(defines["API_OUTGOING_CONNECTION_HOST"]) == '"192.168.1.2"'
    assert str(defines["API_OUTGOING_CONNECTION_PORT"]) == "6054"
    assert str(defines["API_OUTGOING_CONNECTION_DELAY"]) == "60000"


def test_outgoing_connection_defaults(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(_api_config({"host": "192.168.1.2"}))
    outgoing = config["outgoing_connection"]
    assert outgoing["port"] == 6054
    assert outgoing["delay"].total_milliseconds == 60000


def test_outgoing_connection_requires_encryption(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="requires 'encryption'"):
        CONFIG_SCHEMA(_api_config({"host": "192.168.1.2"}, encryption=False))


@pytest.mark.parametrize(
    "platform_framework",
    [PlatformFramework.ESP8266_ARDUINO, PlatformFramework.RP2040_ARDUINO],
)
def test_outgoing_connection_rejected_on_raw_lwip_platforms(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
) -> None:
    set_core_config(platform_framework)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        CONFIG_SCHEMA(_api_config({"host": "192.168.1.2"}))


def test_outgoing_connection_rejects_hostnames(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="not a valid IP address"):
        CONFIG_SCHEMA(_api_config({"host": "homeassistant.local"}))
