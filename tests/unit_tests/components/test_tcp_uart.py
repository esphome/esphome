"""Schema rules for max_connections and turn_timeout on tcp_uart."""

import pytest

from esphome.components import socket
from esphome.components.tcp_uart import CONFIG_SCHEMA
import esphome.config_validation as cv


def test_server_without_max_connections_keeps_one() -> None:
    config = CONFIG_SCHEMA({"role": "server", "port": 502})
    assert config["max_connections"] == 1
    assert socket.get_socket_counts().tcp == 1


def test_server_counts_a_socket_per_connection() -> None:
    config = CONFIG_SCHEMA({"role": "server", "port": 502, "max_connections": 4})
    assert config["max_connections"] == 4
    assert socket.get_socket_counts().tcp == 4
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


def test_turn_timeout_with_one_connection_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="turn_timeout needs max_connections"):
        CONFIG_SCHEMA({"role": "server", "port": 502, "turn_timeout": "200ms"})


def test_turn_timeout_of_zero_is_rejected() -> None:
    # With 0 the turn would end before the reply to its client is written.
    with pytest.raises(cv.Invalid, match="higher than 0s"):
        CONFIG_SCHEMA(
            {"role": "server", "port": 502, "max_connections": 2, "turn_timeout": "0s"}
        )


def test_more_than_eight_connections_are_rejected() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"role": "server", "port": 502, "max_connections": 9})


def test_client_has_no_max_connections() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {"role": "client", "host": "192.0.2.10", "port": 502, "max_connections": 2}
        )
