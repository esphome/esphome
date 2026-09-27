"""Tests for the ipv6_only validation shared by wifi and ethernet."""

import pytest
from voluptuous import Invalid

from esphome.components.const import CONF_IPV6_ONLY
from esphome.components.network import validate_ipv6_only
from esphome.const import (
    CONF_ENABLE_IPV6,
    CONF_MANUAL_IP,
    CONF_MIN_IPV6_ADDR_COUNT,
    CONF_NETWORKS,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable

GOOD_NETWORK = {CONF_ENABLE_IPV6: True, CONF_MIN_IPV6_ADDR_COUNT: 2}


def _validate(set_core_config: SetCoreConfigCallable, network, config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={"network": network})
    validate_ipv6_only(config)


def test_off_is_not_checked(set_core_config: SetCoreConfigCallable) -> None:
    _validate(set_core_config, {}, {CONF_IPV6_ONLY: False})


def test_valid(set_core_config: SetCoreConfigCallable) -> None:
    _validate(set_core_config, GOOD_NETWORK, {CONF_IPV6_ONLY: True})


def test_needs_enable_ipv6(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(Invalid, match="enable_ipv6"):
        _validate(
            set_core_config,
            {CONF_MIN_IPV6_ADDR_COUNT: 2},
            {CONF_IPV6_ONLY: True},
        )


def test_needs_min_ipv6_addr_count(set_core_config: SetCoreConfigCallable) -> None:
    with pytest.raises(Invalid, match="min_ipv6_addr_count"):
        _validate(
            set_core_config,
            {CONF_ENABLE_IPV6: True, CONF_MIN_IPV6_ADDR_COUNT: 0},
            {CONF_IPV6_ONLY: True},
        )


@pytest.mark.parametrize(
    "config",
    [
        {CONF_IPV6_ONLY: True, CONF_MANUAL_IP: {}},
        {CONF_IPV6_ONLY: True, CONF_NETWORKS: [{CONF_MANUAL_IP: {}}]},
    ],
)
def test_rejects_manual_ip(set_core_config: SetCoreConfigCallable, config) -> None:
    with pytest.raises(Invalid, match="manual_ip"):
        _validate(set_core_config, GOOD_NETWORK, config)
