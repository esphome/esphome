"""Tests for the socket component's IPv4 allow list codegen helper."""

from ipaddress import IPv4Address, IPv4Network
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import socket
import esphome.config_validation as cv


def test_network_order_swaps_to_sockaddr_value() -> None:
    """The emitted uint32 must equal s_addr on the little endian targets."""
    assert socket._network_order(IPv4Address("192.168.175.20")) == 0x14AFA8C0
    assert socket._network_order(IPv4Address("255.255.255.0")) == 0x00FFFFFF
    assert socket._network_order(IPv4Address("0.0.0.0")) == 0


def test_add_ipv4_allow_emits_nothing_for_an_empty_list() -> None:
    setter = MagicMock()
    with patch.object(socket.cg, "add") as add:
        socket.add_ipv4_allow(setter, [], "bridge")
    add.assert_not_called()
    setter.assert_not_called()


def test_add_ipv4_allow_wires_the_setter_with_cleared_host_bits() -> None:
    setter = MagicMock()
    networks = [IPv4Network("192.168.175.33/24", strict=False)]
    with (
        patch.object(socket.cg, "add") as add,
        patch.object(socket.cg, "progmem_array") as array,
    ):
        socket.add_ipv4_allow(setter, networks, "bridge")
    rendered = str(array.call_args.args[1])
    assert str(socket._network_order(IPv4Address("192.168.175.0"))) in rendered
    assert str(socket._network_order(IPv4Address("255.255.255.0"))) in rendered
    setter.assert_called_once_with(array.return_value, 1)
    add.assert_called_once()


def test_schema_caps_the_list_length() -> None:
    """The sanity cap rejects a list past 255 entries."""
    assert len(socket.IPV4_ALLOW_SCHEMA([f"10.0.{i}.0/24" for i in range(255)])) == 255
    with pytest.raises(cv.Invalid):
        socket.IPV4_ALLOW_SCHEMA([f"10.0.{i}.0/24" for i in range(256)])
