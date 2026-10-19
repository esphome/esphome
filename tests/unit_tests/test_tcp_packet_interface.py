"""Unit tests for the tcp packet interface platform."""

import pytest

from esphome import config_validation as cv
from esphome.components import socket
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.tcp import packet_interface as tcp_packet_interface
from esphome.core import CORE


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
    with pytest.raises(cv.Invalid, match=f"provides '{layer}' itself"):
        tcp_packet_interface.CONFIG_SCHEMA(
            {"host": "192.168.1.50", "port": 6060, layer: {"address": 1}}
        )


def test_auto_load_binary_sensor_for_connected(setup_core) -> None:
    """The binary_sensor component is loaded only for the connected sensor."""
    assert tcp_packet_interface.AUTO_LOAD() == ["socket", "binary_sensor"]
    assert tcp_packet_interface.AUTO_LOAD({}) == ["socket", "binary_sensor"]
    config = {"host": "192.168.1.50", "port": 6060}
    assert tcp_packet_interface.AUTO_LOAD(config) == ["socket"]
    config["connected"] = {"name": "Link"}
    assert tcp_packet_interface.AUTO_LOAD(config) == ["socket", "binary_sensor"]


def test_timeout(setup_core) -> None:
    """The timeout follows keep_alive unless given, and must be longer than it."""
    base = {"host": "192.168.1.50", "port": 6060}
    config = tcp_packet_interface.CONFIG_SCHEMA({**base, "keep_alive": "2s"})
    assert config["timeout"].total_milliseconds == 6000
    # A peer that sends keep-alives less often is matched with a longer timeout
    config = tcp_packet_interface.CONFIG_SCHEMA({**base, "timeout": "90s"})
    assert config["timeout"].total_milliseconds == 90000
    with pytest.raises(cv.Invalid, match="longer than keep_alive"):
        tcp_packet_interface.CONFIG_SCHEMA(
            {**base, "keep_alive": "5s", "timeout": "5s"}
        )
    with pytest.raises(cv.Invalid):
        tcp_packet_interface.CONFIG_SCHEMA({**base, "keep_alive": "2h"})
    with pytest.raises(cv.Invalid):
        tcp_packet_interface.CONFIG_SCHEMA({**base, "timeout": "4h"})


@pytest.mark.parametrize(
    ("role", "tcp", "listen"), [("client", 1, 0), ("server", 1, 1)]
)
def test_sockets_reserved(setup_core, role: str, tcp: int, listen: int) -> None:
    """A client reserves one socket; a server also reserves a listener."""
    config = {"role": role, "port": 6060}
    if role == "client":
        config["host"] = "192.168.1.50"
    tcp_packet_interface.CONFIG_SCHEMA(config)
    name = "tcp.packet_interface"
    assert CORE.data[socket.KEY_SOCKET_CONSUMERS_TCP][name] == tcp
    assert (
        CORE.data.get(socket.KEY_SOCKET_CONSUMERS_TCP_LISTEN, {}).get(name, 0) == listen
    )
