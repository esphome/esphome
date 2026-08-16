"""Tests for UDP component config validation (Zephyr vs IPv4 platforms)."""

import pytest

from esphome import config_validation as cv
from esphome.components.udp import validate_listen_address, validate_udp_address
from esphome.const import PlatformFramework


def test_zephyr_ipv6_address_ok(set_core_config) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    assert str(validate_udp_address("2001:db8::1")) == "2001:db8::1"


def test_zephyr_ipv4_address_rejected(set_core_config) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    with pytest.raises(cv.Invalid, match="not a valid IPv6 address"):
        validate_udp_address("192.168.1.10")


def test_zephyr_broadcast_default_mapped(set_core_config) -> None:
    """The IPv4 broadcast default must validate on Zephyr (mapped to ff02::1)."""
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    assert str(validate_udp_address("255.255.255.255")) == "ff02::1"


def test_ipv4_platform_ipv4_ok(set_core_config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert str(validate_udp_address("192.168.1.10")) == "192.168.1.10"


def test_ipv4_platform_ipv6_rejected(set_core_config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        validate_udp_address("2001:db8::1")


def test_zephyr_listen_address_rejected(set_core_config) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    with pytest.raises(cv.Invalid, match="not implemented on Zephyr"):
        validate_listen_address("239.0.60.53")


def test_zephyr_listen_address_default_ok(set_core_config) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    assert str(validate_listen_address("255.255.255.255")) == "255.255.255.255"


def test_ipv4_platform_multicast_ok(set_core_config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert str(validate_listen_address("239.0.60.53")) == "239.0.60.53"
