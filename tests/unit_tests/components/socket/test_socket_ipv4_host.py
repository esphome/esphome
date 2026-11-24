"""Tests for the socket component's IPv4 host validator."""

import pytest

from esphome.components import socket
import esphome.config_validation as cv


@pytest.mark.parametrize("value", ["192.0.2.10", "bridge.local", "bridge"])
def test_ipv4_host_accepts_ipv4_address_and_hostname(value: str) -> None:
    assert socket.ipv4_host(value) == value


@pytest.mark.parametrize("value", ["2001:db8::10", "::1", "::ffff:192.0.2.10"])
def test_ipv4_host_rejects_ipv6_address(value: str) -> None:
    with pytest.raises(cv.Invalid, match="IPv6 addresses are not supported"):
        socket.ipv4_host(value)


@pytest.mark.parametrize("value", ["bridge host", ""])
def test_ipv4_host_rejects_invalid_host(value: str) -> None:
    with pytest.raises(cv.Invalid):
        socket.ipv4_host(value)
