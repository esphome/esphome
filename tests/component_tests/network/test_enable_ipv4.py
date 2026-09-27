"""Tests for the network: enable_ipv4 option."""

import pytest
from voluptuous import Invalid

from esphome.components.const import CONF_ENABLE_IPV4
from esphome.components.network import (
    _validate_ipv6_only,
    final_validate_no_manual_ip_if_ipv6_only,
    validate_enable_ipv4,
)
from esphome.const import (
    CONF_ENABLE_IPV6,
    CONF_MANUAL_IP,
    CONF_MIN_IPV6_ADDR_COUNT,
    CONF_NETWORKS,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable

IPV6_ONLY = {
    CONF_ENABLE_IPV4: False,
    CONF_ENABLE_IPV6: True,
    CONF_MIN_IPV6_ADDR_COUNT: 2,
}


def test_enabled_is_not_checked() -> None:
    _validate_ipv6_only({CONF_ENABLE_IPV4: True, CONF_MIN_IPV6_ADDR_COUNT: 0})


def test_valid() -> None:
    _validate_ipv6_only(IPV6_ONLY)


def test_needs_enable_ipv6() -> None:
    with pytest.raises(Invalid, match="enable_ipv6"):
        _validate_ipv6_only({**IPV6_ONLY, CONF_ENABLE_IPV6: False})


def test_needs_min_ipv6_addr_count() -> None:
    with pytest.raises(Invalid, match="min_ipv6_addr_count"):
        _validate_ipv6_only({**IPV6_ONLY, CONF_MIN_IPV6_ADDR_COUNT: 0})


def test_disable_only_on_esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    assert validate_enable_ipv4(True) is True
    with pytest.raises(Invalid, match="only supported on ESP32"):
        validate_enable_ipv4(False)


def test_disable_on_esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert validate_enable_ipv4(False) is False


@pytest.mark.parametrize(
    "config",
    [
        {CONF_MANUAL_IP: {}},
        {CONF_NETWORKS: [{CONF_MANUAL_IP: {}}]},
    ],
)
def test_rejects_manual_ip(set_core_config: SetCoreConfigCallable, config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={"network": IPV6_ONLY})
    with pytest.raises(Invalid, match="manual_ip"):
        final_validate_no_manual_ip_if_ipv6_only(config)


def test_manual_ip_fine_with_ipv4(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={"network": {}})
    final_validate_no_manual_ip_if_ipv6_only({CONF_MANUAL_IP: {}})
