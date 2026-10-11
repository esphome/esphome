"""Tests for the udp address validators."""

from ipaddress import ip_address

import pytest

from esphome.components.udp import (
    CONF_ADDRESSES,
    CONF_LISTEN_ADDRESS,
    _final_validate,
    _listen_address,
)
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_IPV6, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


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


@pytest.mark.parametrize(
    ("config", "full_config", "message"),
    [
        pytest.param(
            {
                CONF_ADDRESSES: [ip_address("ff12::cafe")],
                CONF_LISTEN_ADDRESS: ip_address("255.255.255.255"),
            },
            {},
            "requires 'enable_ipv6: true'",
            id="ipv6_destination_without_ipv6",
        ),
        pytest.param(
            {
                CONF_ADDRESSES: [ip_address("255.255.255.255")],
                CONF_LISTEN_ADDRESS: ip_address("ff12::cafe"),
            },
            {},
            "requires 'enable_ipv6: true'",
            id="ipv6_listen_without_ipv6",
        ),
    ],
)
def test_final_validate_rejects_ipv6_without_network_support(
    set_core_config: SetCoreConfigCallable,
    config: dict,
    full_config: dict,
    message: str,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)
    with pytest.raises(cv.Invalid, match=message):
        _final_validate(config)


def test_final_validate_accepts_ipv6_when_enabled(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"network": {CONF_ENABLE_IPV6: True}},
    )
    _final_validate(
        {
            CONF_ADDRESSES: [ip_address("ff12::cafe")],
            CONF_LISTEN_ADDRESS: ip_address("ff12::cafe"),
        }
    )


def test_final_validate_rejects_ipv6_listen_on_esp8266(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP8266_ARDUINO,
        full_config={"network": {CONF_ENABLE_IPV6: True}},
    )
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _final_validate(
            {
                CONF_ADDRESSES: [ip_address("255.255.255.255")],
                CONF_LISTEN_ADDRESS: ip_address("ff12::cafe"),
            }
        )
