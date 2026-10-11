"""Tests for the udp listen_address validator."""

import pytest

from esphome.components.udp import _listen_address
import esphome.config_validation as cv


@pytest.mark.parametrize("value", ["239.0.60.53", "255.255.255.255", "ff12::cafe"])
def test_listen_address_accepts_multicast_and_broadcast(value: str) -> None:
    assert str(_listen_address(value)) == value


@pytest.mark.parametrize(
    ("value", "message"),
    [
        # An IPv4 address must report the IPv4 error, not an IPv6 parse failure
        ("192.168.1.10", "not a multicast address nor local broadcast address"),
        ("fe80::1", "not an IPv6 multicast address"),
        ("gg::1", "not a valid IPv6 address"),
    ],
)
def test_listen_address_reports_matching_family_error(value: str, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _listen_address(value)
