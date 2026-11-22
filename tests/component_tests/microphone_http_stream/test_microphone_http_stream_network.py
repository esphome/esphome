"""Check the network settings the microphone HTTP stream asks for."""

from collections.abc import Callable
from typing import Any

from esphome.components.microphone_http_stream import _request_wifi_networking
from esphome.components.network import KEY_HIGH_PERFORMANCE_NETWORKING
from esphome.const import CONF_ETHERNET, CONF_WIFI
from esphome.core import CORE


def test_wifi_requests_high_performance_networking(
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The default TCP send buffer is too small for a stream over Wi-Fi."""
    set_component_config(CONF_WIFI, {})
    _request_wifi_networking({})

    assert CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING) is True


def test_ethernet_only_keeps_default_networking(
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Ethernet streams on the default buffers, which stay in internal RAM."""
    set_component_config(CONF_ETHERNET, {})
    _request_wifi_networking({})

    assert KEY_HIGH_PERFORMANCE_NETWORKING not in CORE.data
