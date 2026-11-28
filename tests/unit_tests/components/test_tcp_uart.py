"""Schema rules for max_connections and turn_timeout on tcp_uart."""

from typing import Any

import pytest

from esphome.components import socket
from esphome.components.tcp_uart import CONFIG_SCHEMA
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("config", "max_connections", "tcp_sockets"),
    [
        ({"role": "server", "port": 502}, 1, 1),
        # 4 connections, plus 1 for the new client a full pool accepts before it closes one.
        ({"role": "server", "port": 502, "max_connections": 4}, 4, 5),
    ],
)
def test_server_counts_a_socket_per_connection(
    config: dict[str, Any], max_connections: int, tcp_sockets: int
) -> None:
    validated = CONFIG_SCHEMA(config)
    assert validated["max_connections"] == max_connections
    assert socket.get_socket_counts().tcp == tcp_sockets
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


@pytest.mark.parametrize(
    "config",
    [
        {"role": "server", "port": 502, "max_connections": 9},
        {"role": "server", "port": 502, "max_connections": 2, "turn_timeout": "0s"},
        {
            "role": "server",
            "port": 502,
            "max_connections": 2,
            "turn_timeout": "65536ms",
        },
        {"role": "client", "host": "10.0.0.1", "port": 502, "turn_timeout": "200ms"},
        {"role": "server", "port": 502, "turn_timeout": "200ms"},
    ],
    ids=[
        "too_many_connections",
        "zero_turn_timeout",
        "long_turn_timeout",
        "client",
        "single_connection",
    ],
)
def test_invalid_config_is_rejected(config: dict[str, Any]) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(config)
