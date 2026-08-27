"""Tests for the ``network: tcp_send_buffer:`` option.

The option sets lwIP's per-socket TCP send buffer
(CONFIG_LWIP_TCP_SND_BUF_DEFAULT) on ESP-IDF. The stock default (5744 bytes)
stalls bursty senders such as a Bluetooth proxy streaming GATT notifications;
until now the only way to raise it was the all-or-nothing
``enable_high_performance`` bundle.
"""

from collections.abc import Callable
from pathlib import Path

import pytest
from voluptuous import Invalid

from esphome import config_validation as cv
from esphome.components.esp32.const import (
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
    VARIANT_ESP32,
)
from esphome.components.network import (
    CONF_TCP_SEND_BUFFER,
    CONFIG_SCHEMA,
    TCP_SEND_BUFFER_MAX,
    TCP_SEND_BUFFER_MIN,
)
from esphome.const import KEY_ESP32, KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


def _sdkconfig_option(name: str) -> int | None:
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS].get(name)


def test_tcp_send_buffer_sets_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("tcp_send_buffer.yaml"))
    assert _sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT") == 32000


def test_tcp_send_buffer_overrides_high_performance(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An explicit size wins over the high performance bundle's 65534."""
    generate_main(component_config_path("tcp_send_buffer_high_perf.yaml"))
    assert _sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT") == 16384


@pytest.mark.parametrize("value", [TCP_SEND_BUFFER_MIN, TCP_SEND_BUFFER_MAX])
def test_boundary_values_accepted(
    set_core_config: SetCoreConfigCallable, value: int
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
        platform_data={KEY_VARIANT: VARIANT_ESP32},
    )
    assert CONFIG_SCHEMA({"tcp_send_buffer": value})[CONF_TCP_SEND_BUFFER] == value


@pytest.mark.parametrize("value", ["1kB", "128kB"])
def test_out_of_range_rejected(
    set_core_config: SetCoreConfigCallable, value: str
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 5)},
        platform_data={KEY_VARIANT: VARIANT_ESP32},
    )
    with pytest.raises(Invalid):
        CONFIG_SCHEMA({"tcp_send_buffer": value})


def test_rejected_on_esp8266(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP8266_ARDUINO,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(3, 1, 2)},
    )
    with pytest.raises(Invalid, match="esp32"):
        CONFIG_SCHEMA({"tcp_send_buffer": "32kB"})
