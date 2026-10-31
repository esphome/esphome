"""Unit tests for the udp packet interface platform."""

from ipaddress import IPv4Address, IPv6Address

import pytest

from esphome import config_validation as cv
from esphome.components.udp import (
    CONF_ADDRESSES,
    packet_interface as udp_packet_interface,
)
from esphome.core import ID
import esphome.final_validate as fv


@pytest.mark.parametrize(
    ("addresses", "point_to_point", "peer"),
    [
        ([IPv4Address("192.168.1.60")], None, IPv4Address("192.168.1.60")),
        ([IPv4Address("255.255.255.255")], None, None),
        ([IPv4Address("192.168.1.255")], None, None),
        ([IPv4Address("239.0.60.53")], None, None),
        ([IPv4Address("192.168.1.60"), IPv4Address("192.168.1.61")], None, None),
        ([IPv6Address("2001:db8::1")], None, None),
        # A directed broadcast on a /25, and a host ending in .255 on a /23
        ([IPv4Address("192.168.1.127")], False, None),
        ([IPv4Address("192.168.0.255")], True, IPv4Address("192.168.0.255")),
    ],
)
def test_peer(
    addresses: list, point_to_point: bool | None, peer: IPv4Address | None
) -> None:
    """Only one unicast IPv4 destination makes the link point to point, unless point_to_point says otherwise."""
    config = {} if point_to_point is None else {"point_to_point": point_to_point}
    assert udp_packet_interface._peer({CONF_ADDRESSES: addresses}, config) == peer


@pytest.mark.parametrize(
    "addresses",
    [
        [IPv4Address("239.0.60.53")],
        [IPv4Address("192.168.1.60"), IPv4Address("192.168.1.61")],
    ],
)
def test_point_to_point_needs_one_unicast_address(addresses: list) -> None:
    """point_to_point cannot make a multicast or several addresses into one peer."""
    with pytest.raises(cv.Invalid, match="one unicast IPv4 address"):
        udp_packet_interface._peer(
            {CONF_ADDRESSES: addresses}, {"point_to_point": True}
        )


def _final_validate(addresses: list, layers: dict) -> None:
    config = udp_packet_interface.CONFIG_SCHEMA({"udp_id": "udp_link", **layers})
    token = fv.full_config.set(
        {"udp": [{"id": ID("udp_link"), CONF_ADDRESSES: addresses}]}
    )
    try:
        udp_packet_interface._final_validate(config)
    finally:
        fv.full_config.reset(token)


def test_point_to_point_rejects_addressing(setup_core) -> None:
    """Packets to one unicast address are already addressed."""
    addressing = {"addressing": {"address": 1, "destination": 2}}
    with pytest.raises(cv.Invalid, match="already addressed"):
        _final_validate([IPv4Address("192.168.1.60")], addressing)
    _final_validate([IPv4Address("192.168.1.255")], addressing)
    _final_validate(
        [IPv4Address("192.168.1.127")], {**addressing, "point_to_point": False}
    )


def test_broadcast_needs_addressing_for_two_device_layers(setup_core) -> None:
    """Every device hears a broadcast link, so reliability needs an addressing layer there."""
    with pytest.raises(cv.Invalid, match="every device hears this link"):
        _final_validate([IPv4Address("192.168.1.255")], {"reliability": None})
    _final_validate([IPv4Address("192.168.1.60")], {"reliability": None})
