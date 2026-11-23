"""Tests for the udp address validators."""

from ipaddress import ip_address

import pytest

from esphome.components import network
from esphome.components.udp import (
    CONF_ADDRESSES,
    CONF_LISTEN_ADDRESS,
    _address,
    _final_validate,
    _listen_address,
    _require_ipv6_for_ipv6_addresses,
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
        ("ff02::cafe%eth0", "carries a scope id"),
        ("gg::1", "not a valid IPv6 address"),
    ],
)
def test_listen_address_reports_matching_family_error(value: str, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _listen_address(value)


@pytest.mark.parametrize(
    "value", ["10.0.0.1", "255.255.255.255", "ff12::cafe", "2001:db8::1"]
)
def test_address_accepts_routable_destinations(value: str) -> None:
    assert str(_address(value)) == value


@pytest.mark.parametrize(
    ("value", "message"),
    [
        # The device parses addresses with inet_pton, which takes no scope suffix
        ("2001:db8::1%wlan0", "carries a scope id"),
        # No scope id is set on the destination, so a link-local send would fail
        ("fe80::1", "is link-local"),
    ],
)
def test_address_rejects_unsendable_destinations(value: str, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _address(value)


_RAW_LWIP_IPV6 = {
    "network": {CONF_ENABLE_IPV6: True},
    "socket": {"implementation": "lwip_tcp"},
}
_RAW_LWIP = "not supported on this platform"


@pytest.mark.parametrize(
    ("addresses", "listen_address", "platform", "full_config", "message"),
    [
        pytest.param(
            ["ff12::cafe"],
            "255.255.255.255",
            PlatformFramework.ESP8266_ARDUINO,
            _RAW_LWIP_IPV6,
            _RAW_LWIP,
            id="ipv6_destination_on_raw_lwip",
        ),
        pytest.param(
            ["255.255.255.255"],
            "ff12::cafe",
            PlatformFramework.ESP8266_ARDUINO,
            _RAW_LWIP_IPV6,
            _RAW_LWIP,
            id="ipv6_listen_on_raw_lwip",
        ),
    ],
)
def test_final_validate_rejects_unsupported_ipv6(
    set_core_config: SetCoreConfigCallable,
    addresses: list[str],
    listen_address: str,
    platform: PlatformFramework,
    full_config: dict,
    message: str,
) -> None:
    set_core_config(platform, full_config=full_config)
    with pytest.raises(cv.Invalid, match=message):
        _final_validate(
            {
                CONF_ADDRESSES: [ip_address(a) for a in addresses],
                CONF_LISTEN_ADDRESS: ip_address(listen_address),
            }
        )


@pytest.mark.parametrize(
    ("addresses", "listen_address", "required"),
    [
        (["ff12::cafe"], "255.255.255.255", True),
        (["255.255.255.255"], "ff12::cafe", True),
        (["255.255.255.255"], "255.255.255.255", False),
    ],
)
def test_ipv6_address_requires_ipv6_from_network(
    set_core_config: SetCoreConfigCallable,
    addresses: list[str],
    listen_address: str,
    required: bool,
) -> None:
    """An IPv6 destination or listen address registers udp with network.require_ipv6()."""
    set_core_config(PlatformFramework.ESP32_IDF)
    _require_ipv6_for_ipv6_addresses(
        {
            CONF_ADDRESSES: [ip_address(a) for a in addresses],
            CONF_LISTEN_ADDRESS: ip_address(listen_address),
        }
    )
    assert ("udp" in network._get_data().require_ipv6) is required


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
