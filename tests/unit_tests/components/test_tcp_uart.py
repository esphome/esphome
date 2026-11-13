"""Schema rules for max_connections and turn_timeout on tcp_uart."""

from esphome.components import socket
from esphome.components.tcp_uart import CONFIG_SCHEMA


def test_server_without_max_connections_keeps_one() -> None:
    config = CONFIG_SCHEMA({"role": "server", "port": 502})
    assert config["max_connections"] == 1
    assert socket.get_socket_counts().tcp == 1


def test_server_counts_a_socket_per_connection_and_one_spare() -> None:
    config = CONFIG_SCHEMA({"role": "server", "port": 502, "max_connections": 4})
    assert config["max_connections"] == 4
    # 4 connections, plus 1 for the new client a full pool accepts before it closes one.
    assert socket.get_socket_counts().tcp == 5
    assert socket.get_socket_counts().tcp_listen == 1


def test_turn_timeout_with_several_connections_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        {
            "role": "server",
            "port": 502,
            "max_connections": 2,
            "turn_timeout": "200ms",
        }
    )
    assert config["turn_timeout"].total_milliseconds == 200
