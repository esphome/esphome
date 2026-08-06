"""The three platform-gate branches: BLE-less platforms are rejected with the
real reason, hub platforms reject GATT-only options by name, and the
advertisement-only arm applies its own defaults."""

import pytest

from esphome import config_validation as cv
from esphome.components import bluetooth_proxy
from esphome.const import CONF_ACTIVE, KEY_TARGET_PLATFORM
from esphome.core import CORE, KEY_CORE


def _set_platform(platform: str) -> None:
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = platform


def test_ble_less_platform_gets_the_real_reason() -> None:
    _set_platform("esp8266")
    with pytest.raises(cv.Invalid, match="not supported on esp8266"):
        bluetooth_proxy.CONFIG_SCHEMA({})


def test_ble_less_platform_connection_keys_fall_through() -> None:
    # The key-level rejection must not fire here — it would imply an
    # advertisement-only proxy exists on this platform.
    _set_platform("esp8266")
    with pytest.raises(cv.Invalid, match="not supported on esp8266"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})


def test_hub_platform_rejects_active() -> None:
    _set_platform("ln882x")
    with pytest.raises(cv.Invalid, match="Active connections are not supported"):
        bluetooth_proxy.CONFIG_SCHEMA({"active": True})


def test_hub_platform_rejects_connection_keys_by_name() -> None:
    _set_platform("ln882x")
    with pytest.raises(cv.Invalid, match="'connection_slots' requires active"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})
    with pytest.raises(cv.Invalid, match="'cache_services' requires active"):
        bluetooth_proxy.CONFIG_SCHEMA({"cache_services": True})


def test_hub_platform_accepts_the_advertisement_only_shape() -> None:
    _set_platform("ln882x")
    validated = bluetooth_proxy.CONFIG_SCHEMA({})
    assert validated[CONF_ACTIVE] is False
