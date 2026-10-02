"""Tests for the socket component's source filtering and require functions."""

from unittest.mock import patch

from esphome.components import socket
from esphome.core import Define


def test_helper_files_filtered_until_required() -> None:
    """ipv4_resolve.cpp and tcp_client_link.cpp compile only when required."""
    with patch("esphome.config_helpers.CORE") as mock_core:
        mock_core.defines = set()
        filtered = socket.FILTER_SOURCE_FILES()
        assert "ipv4_resolve.cpp" in filtered
        assert "tcp_client_link.cpp" in filtered

        mock_core.defines = {Define("USE_SOCKET_IPV4_RESOLVE")}
        filtered = socket.FILTER_SOURCE_FILES()
        assert "ipv4_resolve.cpp" not in filtered
        assert "tcp_client_link.cpp" in filtered

        mock_core.defines = {
            Define("USE_SOCKET_IPV4_RESOLVE"),
            Define("USE_SOCKET_TCP_CLIENT_LINK"),
        }
        filtered = socket.FILTER_SOURCE_FILES()
        assert "ipv4_resolve.cpp" not in filtered
        assert "tcp_client_link.cpp" not in filtered


def test_require_tcp_client_link_pulls_in_the_resolver() -> None:
    """require_tcp_client_link() sets both defines; the link reads the resolver."""
    with patch.object(socket.cg, "add_define") as add_define:
        socket.require_tcp_client_link()
    assert {call.args[0] for call in add_define.call_args_list} == {
        "USE_SOCKET_IPV4_RESOLVE",
        "USE_SOCKET_TCP_CLIENT_LINK",
    }
