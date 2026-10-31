"""Check the network settings the microphone HTTP stream asks for."""

from esphome.components.microphone_http_stream import (
    CONF_MAX_CLIENTS,
    _register_httpd_clients,
)
from esphome.components.network import KEY_HIGH_PERFORMANCE_NETWORKING
from esphome.core import CORE


def test_requests_high_performance_networking() -> None:
    """The default TCP send buffer is too small for a stream over Wi-Fi."""
    _register_httpd_clients({CONF_MAX_CLIENTS: 1})

    assert CORE.data[KEY_HIGH_PERFORMANCE_NETWORKING] is True
