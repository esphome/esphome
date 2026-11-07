"""Tests for the api outgoing_connection option."""

from collections.abc import Callable
from ipaddress import IPv4Address
from pathlib import Path

import pytest

from esphome.components.api import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
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


def test_outgoing_connection_bare_block(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A bare outgoing_connection: block is valid; the device dials the
    remembered last dial-back client."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(_api_config(None))
    outgoing = config["outgoing_connection"]
    assert "host" not in outgoing
    assert outgoing["port"] == 6054


@pytest.mark.parametrize(
    ("delay", "reboot_timeout"),
    [("16min", None), ("30s", "30s"), ("31s", "30s")],
)
def test_outgoing_connection_delay_bounded(
    set_core_config: SetCoreConfigCallable,
    delay: str,
    reboot_timeout: str | None,
) -> None:
    """A delay the reboot watchdog would cut short is rejected."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = _api_config({"delay": delay})
    if reboot_timeout is not None:
        config["reboot_timeout"] = reboot_timeout
    with pytest.raises(cv.Invalid, match="shorter than reboot_timeout"):
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    ("delay", "reboot_timeout"),
    [("20min", "1h"), ("29s", "30s"), ("30min", "0s")],
)
def test_outgoing_connection_delay_within_timeout(
    set_core_config: SetCoreConfigCallable,
    delay: str,
    reboot_timeout: str,
) -> None:
    """The bound follows the configured timeout, and a disabled one has none."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = _api_config({"delay": delay}) | {"reboot_timeout": reboot_timeout}
    assert CONFIG_SCHEMA(config)["outgoing_connection"]["delay"] == cv.time_period(
        delay
    )


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
def test_outgoing_connection_accepts_raw_lwip(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
) -> None:
    """The raw lwip_tcp socket these platforms default to can dial out."""
    set_core_config(platform_framework)
    config = CONFIG_SCHEMA(_api_config({"host": "192.168.1.2"}))
    assert FINAL_VALIDATE_SCHEMA(config) is not None


def test_outgoing_connection_rejects_hostnames(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="not a valid IP address"):
        CONFIG_SCHEMA(_api_config({"host": "homeassistant.local"}))


def test_outgoing_connection_rejects_scope_id(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A scope id passes Python's parser but not the device's."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="scope id"):
        CONFIG_SCHEMA(_api_config({"host": "fe80::1%eth0"}))


def test_outgoing_connection_folds_v4_mapped_host(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A v4-mapped host becomes plain IPv4, so it needs no IPv6 build."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(_api_config({"host": "::ffff:192.168.1.2"}))
    assert config["outgoing_connection"]["host"] == IPv4Address("192.168.1.2")
    assert FINAL_VALIDATE_SCHEMA(config) is not None


def test_outgoing_connection_ipv6_host_requires_ipv6(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(_api_config({"host": "fd00::1"}))
    with pytest.raises(cv.Invalid, match="IPv6 is not"):
        FINAL_VALIDATE_SCHEMA(config)


def test_outgoing_connection_ipv6_host_passes_with_ipv6_enabled(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data=ESP32_PLATFORM_DATA,
        full_config={"network": {"enable_ipv6": True}},
    )
    config = CONFIG_SCHEMA(_api_config({"host": "fd00::1"}))
    assert FINAL_VALIDATE_SCHEMA(config) is not None
