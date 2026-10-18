"""Unit tests for the udp packet interface platform."""

from ipaddress import IPv4Address, IPv6Address

import pytest

from esphome.components.udp import (
    CONF_ADDRESSES,
    packet_interface as udp_packet_interface,
)


@pytest.mark.parametrize(
    ("addresses", "peer"),
    [
        ([IPv4Address("192.168.1.60")], IPv4Address("192.168.1.60")),
        ([IPv4Address("255.255.255.255")], None),
        ([IPv4Address("192.168.1.255")], None),
        ([IPv4Address("239.0.60.53")], None),
        ([IPv4Address("192.168.1.60"), IPv4Address("192.168.1.61")], None),
        ([IPv6Address("2001:db8::1")], None),
    ],
)
def test_peer(addresses: list, peer: IPv4Address | None) -> None:
    """Only one unicast IPv4 destination makes the link point to point."""
    assert udp_packet_interface._peer({CONF_ADDRESSES: addresses}) == peer
