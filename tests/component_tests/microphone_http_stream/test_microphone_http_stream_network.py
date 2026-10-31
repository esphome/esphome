"""Check the network settings the microphone HTTP stream asks for."""

from collections.abc import Generator

import pytest

from esphome.components.microphone_http_stream import _request_wifi_networking
from esphome.components.network import KEY_HIGH_PERFORMANCE_NETWORKING
from esphome.config import Config
from esphome.const import CONF_ETHERNET, CONF_WIFI
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.fixture
def full_config() -> Generator[Config, None, None]:
    config = Config()
    token = fv.full_config.set(config)
    yield config
    fv.full_config.reset(token)


def test_wifi_requests_high_performance_networking(full_config: Config) -> None:
    """The default TCP send buffer is too small for a stream over Wi-Fi."""
    full_config[CONF_WIFI] = {}
    _request_wifi_networking({})

    assert CORE.data.get(KEY_HIGH_PERFORMANCE_NETWORKING) is True


def test_ethernet_only_keeps_default_networking(full_config: Config) -> None:
    """Ethernet streams on the default buffers, which stay in internal RAM."""
    full_config[CONF_ETHERNET] = {}
    _request_wifi_networking({})

    assert KEY_HIGH_PERFORMANCE_NETWORKING not in CORE.data
