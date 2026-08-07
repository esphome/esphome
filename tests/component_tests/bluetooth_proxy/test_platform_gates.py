"""The three platform-gate branches: BLE-less platforms are rejected with the
real reason, hub platforms reject GATT-only options by name, and the
advertisement-only arm applies its own defaults."""

import pytest

from esphome import config_validation as cv
from esphome.components import ble_device_base, bluetooth_connection, bluetooth_proxy
from esphome.const import CONF_ACTIVE, KEY_TARGET_PLATFORM
from esphome.core import CORE, KEY_CORE


def _set_platform(platform: str | None) -> None:
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = platform


def _set_hub_platform(platform: str, tracker: str) -> None:
    # The ble_hub_id guard needs a loaded tracker, normally registered as an
    # import side effect of the tracker module.
    _set_platform(platform)
    ble_device_base.register_hub_provider(tracker)
    CORE.loaded_integrations.add(tracker)


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
    _set_hub_platform("ln882x", "ln882h_ble_tracker")
    with pytest.raises(cv.Invalid, match="Active connections are not supported"):
        bluetooth_proxy.CONFIG_SCHEMA({"active": True})


def test_hub_platform_rejects_connection_keys_by_name() -> None:
    _set_platform("ln882x")
    with pytest.raises(cv.Invalid, match="'connection_slots' requires active"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})
    with pytest.raises(cv.Invalid, match="'cache_services' requires active"):
        bluetooth_proxy.CONFIG_SCHEMA({"cache_services": True})


def test_hub_platform_accepts_the_advertisement_only_shape() -> None:
    _set_hub_platform("ln882x", "ln882h_ble_tracker")
    validated = bluetooth_proxy.CONFIG_SCHEMA({})
    assert validated[CONF_ACTIVE] is False


def test_rp2_defaults_to_the_full_proxy() -> None:
    # esp32 parity: active defaults to true, with the platform's slot limit,
    # and one populated connection entry for the codegen to index.
    _set_hub_platform("rp2", "rp2_ble_tracker")
    validated = bluetooth_proxy.CONFIG_SCHEMA({})
    assert validated[CONF_ACTIVE] is True
    assert validated[bluetooth_proxy.CONF_CONNECTION_SLOTS] == 1
    assert len(validated[bluetooth_proxy.CONF_CONNECTIONS]) == 1


def test_rp2_accepts_explicit_passive() -> None:
    _set_hub_platform("rp2", "rp2_ble_tracker")
    validated = bluetooth_proxy.CONFIG_SCHEMA({CONF_ACTIVE: False})
    assert validated[CONF_ACTIVE] is False
    assert bluetooth_proxy.CONF_CONNECTIONS not in validated


def test_rp2_rejects_slots_beyond_the_btstack_limit() -> None:
    # The prebuilt BTstack library allows exactly one GATT client connection.
    _set_hub_platform("rp2", "rp2_ble_tracker")
    with pytest.raises(cv.Invalid, match="at most 1 connection slot"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})
    # Values past even the loosest platform cap stop at the outer walkable
    # schema, which stays bounded for range walkers (device-builder sync);
    # in-range values get the platform message above.
    with pytest.raises(cv.Invalid, match="at most 9"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 12})


def test_rp2_rejects_esp32_only_keys_by_name() -> None:
    _set_hub_platform("rp2", "rp2_ble_tracker")
    with pytest.raises(cv.Invalid, match="'cache_services' is esp32-only"):
        bluetooth_proxy.CONFIG_SCHEMA({"cache_services": True})
    with pytest.raises(cv.Invalid, match="'connections' has no per-connection options"):
        bluetooth_proxy.CONFIG_SCHEMA({"connections": [{}]})


def test_bluetooth_connection_auto_load_covers_its_includes() -> None:
    # The esp32 connection header includes esp32_ble_client; the auto load
    # must satisfy that closure itself (regression: it once relied on the
    # consumer's auto loads).
    _set_platform("esp32")
    assert "esp32_ble_client" in bluetooth_connection.AUTO_LOAD()
    _set_platform("rp2")
    assert bluetooth_connection.AUTO_LOAD() == ["ble_device_base"]
    # No target platform (tooling resolving the manifest): the union, so
    # dependency closures stay complete for build_codeowners and friends.
    _set_platform(None)
    assert bluetooth_connection.AUTO_LOAD() == ["ble_device_base", "esp32_ble_client"]


def test_every_registered_hub_platform_has_a_schema_arm() -> None:
    # A platform added to HUB_MAX_CONNECTIONS without a schema builder,
    # codegen arm, or _HUB_PLATFORMS entry would only fail when a config for
    # it is validated (or not even then); pin all three couplings here.
    registered = set(bluetooth_connection.HUB_MAX_CONNECTIONS)
    assert registered <= set(bluetooth_proxy._GATT_HUB_SCHEMAS)
    assert registered <= set(bluetooth_proxy._GATT_HUB_TO_CODE)
    assert registered <= set(bluetooth_proxy._HUB_PLATFORMS)
