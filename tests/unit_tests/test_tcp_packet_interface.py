"""Unit tests for the tcp packet interface platform."""

import pytest

from esphome import config_validation as cv
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.tcp import packet_interface as tcp_packet_interface


def test_client_and_server(setup_core) -> None:
    """A client needs a host; a server listens on its port."""
    client = tcp_packet_interface.CONFIG_SCHEMA({"host": "192.168.1.50", "port": 6060})
    assert client["role"] == "client"
    assert client[CONF_MAX_PACKET_SIZE] == 1022
    assert tcp_packet_interface.CONFIG_SCHEMA({"role": "server", "port": 6060})
    with pytest.raises(cv.Invalid):
        tcp_packet_interface.CONFIG_SCHEMA({"port": 6060})


@pytest.mark.parametrize("layer", ["reliability", "session", "addressing"])
def test_provided_layers_rejected(setup_core, layer: str) -> None:
    """TCP provides these itself, so they cannot be configured on it."""
    with pytest.raises(cv.Invalid, match=layer):
        tcp_packet_interface.CONFIG_SCHEMA(
            {"host": "192.168.1.50", "port": 6060, layer: {"address": 1}}
        )


def test_auto_load_binary_sensor_for_connected(setup_core) -> None:
    """The binary_sensor component is loaded only for the connected sensor."""
    assert tcp_packet_interface.AUTO_LOAD() == ["socket", "binary_sensor"]
    config = {"host": "192.168.1.50", "port": 6060}
    assert tcp_packet_interface.AUTO_LOAD(config) == ["socket"]
    config["connected"] = {"name": "Link"}
    assert tcp_packet_interface.AUTO_LOAD(config) == ["socket", "binary_sensor"]
