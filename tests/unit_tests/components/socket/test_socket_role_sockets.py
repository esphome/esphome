"""Tests for consume_role_sockets(), the socket count of client and server roles."""

from esphome.components import socket


def test_client_counts_one_connection() -> None:
    socket.consume_role_sockets("bridge", "max_connections")({"role": "client"})
    counts = socket.get_socket_counts()
    assert counts.tcp == 1
    assert counts.tcp_listen == 0


def test_server_counts_one_connection_without_the_key() -> None:
    socket.consume_role_sockets("bridge", "max_connections")({"role": "server"})
    counts = socket.get_socket_counts()
    assert counts.tcp == 1
    assert counts.tcp_listen == 1


def test_server_counts_each_connection_and_one_listener() -> None:
    socket.consume_role_sockets("bridge", "max_connections")(
        {"role": "server", "max_connections": 4}
    )
    counts = socket.get_socket_counts()
    assert counts.tcp == 4
    assert counts.tcp_listen == 1
