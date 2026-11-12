"""Tests for consume_role_sockets(), the socket count of client and server roles."""

import pytest

from esphome.components import socket
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("config", "tcp", "tcp_listen"),
    [
        ({"role": "client"}, 1, 0),
        ({"role": "server", "max_connections": 1}, 1, 1),
        ({"role": "server", "max_connections": 4}, 4, 1),
    ],
)
def test_role_socket_counts(config: ConfigType, tcp: int, tcp_listen: int) -> None:
    socket.consume_role_sockets("bridge", "max_connections")(config)
    counts = socket.get_socket_counts()
    assert counts.tcp == tcp
    assert counts.tcp_listen == tcp_listen
