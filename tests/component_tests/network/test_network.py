"""Tests for network component."""

from collections.abc import Callable
from pathlib import Path

import pytest
from voluptuous import Invalid

from esphome.components.network import (
    CONF_ENABLE_IPV4,
    _final_validate,
    _get_data,
    request_ipv4_off,
    require_ipv4,
    require_ipv6,
)
from esphome.config import load_config
from esphome.const import CONF_ENABLE_IPV6, KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.core import CORE, Version
from tests.component_tests.types import SetCoreConfigCallable


def _define_value(name: str) -> str | None:
    define = next((d for d in CORE.defines if d.name == name), None)
    return None if define is None else str(define.value)


def test_require_ipv4_records_name_and_passes_config_through() -> None:
    """require_ipv4() returns the config unchanged and records the caller once."""
    config = {"foo": "bar"}
    validator = require_ipv4("foo")
    assert validator(config) is config
    validator({})
    assert _get_data().require_ipv4 == {"foo"}


def test_require_ipv6_and_request_ipv4_off_record_state() -> None:
    """require_ipv6() and request_ipv4_off() record their callers."""
    require_ipv6("bar")({})
    request_ipv4_off("bar")({})
    data = _get_data()
    assert data.require_ipv6 == {"bar"}
    assert data.request_ipv4_off == {"bar"}


@pytest.mark.parametrize(
    ("config_file", "ipv4", "ipv6"),
    [
        ("ipv6_only_ipv4_explicit_disable.yaml", "false", "true"),
        ("ipv6_only.yaml", "true", "true"),
        ("ipv4_required_with_ipv6.yaml", "true", "true"),
        ("wifi_only.yaml", "true", "false"),
        ("openthread_no_explicit_network.yaml", "false", "true"),
        ("openthread_ipv4_explicitly_on.yaml", "true", "true"),
        ("ipv6_off_nothing_required.yaml", "true", "false"),
        ("ipv4_and_ipv6_both_required.yaml", "true", "true"),
        ("openthread_with_wake_on_lan.yaml", "true", "true"),
        ("esp32_arduino_ipv6_no_ipv4_requirement.yaml", "true", "true"),
        ("host_ipv6_no_ipv4_requirement.yaml", "true", "true"),
        ("nrf52_network_no_openthread.yaml", "false", "true"),
        ("wifi_ipv6_only.yaml", "false", "true"),
        ("ethernet_ipv6_only.yaml", "false", "true"),
    ],
)
def test_ip_family_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    ipv4: str,
    ipv6: str,
) -> None:
    """Each config resolves USE_NETWORK_IPV4 and USE_NETWORK_IPV6 as expected."""
    generate_main(component_config_path(config_file))
    assert _define_value("USE_NETWORK_IPV4") == ipv4
    assert _define_value("USE_NETWORK_IPV6") == ipv6


@pytest.mark.parametrize(
    "config_file",
    [
        "wifi_manual_ip_ipv4_disabled_rejected.yaml",
        "wifi_lease_cache_ipv4_disabled_rejected.yaml",
        "wifi_ap_ipv4_disabled_rejected.yaml",
        "ethernet_manual_ip_ipv4_disabled_rejected.yaml",
        "wake_on_lan_ipv4_disabled_rejected.yaml",
        "udp_ipv4_disabled_rejected.yaml",
        "tcp_uart_ipv4_disabled_rejected.yaml",
        "uart_tcp_ipv4_disabled_rejected.yaml",
        "esp8266_ipv4_disabled_rejected.yaml",
        "esp32_arduino_ipv4_disabled_rejected.yaml",
    ],
)
def test_require_ipv4_registration_rejects_explicit_disable(
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """A component that requires IPv4 rejects an explicit 'enable_ipv4: false'."""
    CORE.config_path = component_config_path(config_file)
    res = load_config({})
    assert any("explicitly disables it" in str(err) for err in res.errors)


@pytest.mark.parametrize(
    ("config_file", "component"),
    [
        ("tcp_uart_ipv4_disabled_rejected.yaml", "tcp_uart"),
        ("uart_tcp_ipv4_disabled_rejected.yaml", "uart_tcp"),
    ],
)
def test_tcp_link_users_named_in_ipv4_error(
    component_config_path: Callable[[str], Path],
    config_file: str,
    component: str,
) -> None:
    """The IPv4 requirement registered by the socket helper names the component."""
    CORE.config_path = component_config_path(config_file)
    res = load_config({})
    assert any(f"IPv4 is required by {component}" in str(err) for err in res.errors)


def test_final_validate_rejects_explicit_ipv6_disable_when_required() -> None:
    """An explicit 'enable_ipv6: false' conflicting with require_ipv6() is rejected."""
    require_ipv6("test")({})
    with pytest.raises(Invalid, match="explicitly disables it"):
        _final_validate({CONF_ENABLE_IPV6: False})


def test_final_validate_accepts_explicit_ipv6_disable_when_not_required() -> None:
    """An explicit 'enable_ipv6: false' is accepted when nothing requires IPv6."""
    _final_validate({CONF_ENABLE_IPV6: False})


def test_final_validate_resolves_ipv6_default_when_required(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """require_ipv6() turns an absent 'enable_ipv6' on."""
    set_core_config(
        PlatformFramework.HOST_NATIVE,
        core_data={KEY_FRAMEWORK_VERSION: Version(0, 0, 0)},
    )
    require_ipv6("test")({})
    config: dict[str, bool] = {}
    _final_validate(config)
    assert config[CONF_ENABLE_IPV6] is True


def test_final_validate_rejects_explicit_ipv4_disable_when_required() -> None:
    """An explicit 'enable_ipv4: false' conflicting with require_ipv4() is rejected."""
    require_ipv4("test")({})
    with pytest.raises(Invalid, match="explicitly disables it"):
        _final_validate({CONF_ENABLE_IPV4: False})


def test_final_validate_accepts_explicit_ipv4_disable_when_not_required() -> None:
    """An explicit 'enable_ipv4: false' is accepted when IPv6 is on."""
    _final_validate({CONF_ENABLE_IPV4: False, CONF_ENABLE_IPV6: True})


def test_final_validate_accepts_ipv4_default_when_required() -> None:
    """An explicit 'enable_ipv4: true' never conflicts with require_ipv4()."""
    require_ipv4("test")({})
    _final_validate({CONF_ENABLE_IPV4: True})


@pytest.mark.parametrize(
    "config",
    [{CONF_ENABLE_IPV4: False}, {CONF_ENABLE_IPV4: False, CONF_ENABLE_IPV6: False}],
)
def test_final_validate_rejects_both_address_families_disabled(
    config: dict[str, bool],
) -> None:
    """Disabling IPv4 with IPv6 off leaves no IP stack and is rejected."""
    with pytest.raises(Invalid, match="leaves no IP stack"):
        _final_validate(config)


@pytest.mark.parametrize(
    "config_file",
    [
        "esp8266_no_network.yaml",
        "rp2_no_network.yaml",
        "bk72xx_no_network.yaml",
        "esp32_idf_no_network.yaml",
        "esp32_arduino_no_network.yaml",
        "nrf52_no_network.yaml",
    ],
)
def test_no_ip_defines_without_network_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Platform IP requests emit no IP family defines when network is not loaded."""
    generate_main(component_config_path(config_file))
    assert _define_value("USE_NETWORK_IPV4") is None
    assert _define_value("USE_NETWORK_IPV6") is None
