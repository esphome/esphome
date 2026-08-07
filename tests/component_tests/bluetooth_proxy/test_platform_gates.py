"""The three platform-gate branches: BLE-less platforms are rejected with the
real reason, hub platforms reject GATT-only options by name, and the
advertisement-only arm applies its own defaults."""

import pytest

from esphome import config_validation as cv
from esphome.components import ble_device_base, bluetooth_connection, bluetooth_proxy
from esphome.const import (
    CONF_ACTIVE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PlatformFramework,
)
from esphome.core import CORE

from ..types import SetCoreConfigCallable

HUB_PLATFORM_FRAMEWORKS = [
    PlatformFramework.LN882X_ARDUINO,
    PlatformFramework.RP2_ARDUINO,
]

HUB_TRACKERS = {
    PLATFORM_LN882X: "ln882h_ble_tracker",
    PLATFORM_RP2: "rp2_ble_tracker",
}


def test_hub_platform_list_covers_every_hub_platform() -> None:
    # A platform added to _HUB_PLATFORMS (bk72xx is planned) would otherwise
    # get no gate coverage at all.
    covered = {pf.value[0] for pf in HUB_PLATFORM_FRAMEWORKS}
    assert covered == set(bluetooth_proxy._HUB_PLATFORMS)
    assert set(HUB_TRACKERS) == set(bluetooth_proxy._HUB_PLATFORMS)


def _set_platform(platform: str | None) -> None:
    # For arms set_core_config cannot express (bare platform, no framework).
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = platform


def _register_tracker(platform: str) -> None:
    # The ble_hub_id guard needs a loaded tracker, normally registered as an
    # import side effect of the tracker module.
    tracker = HUB_TRACKERS[platform]
    ble_device_base.register_hub_provider(tracker)
    CORE.loaded_integrations.add(tracker)


def test_ble_less_platform_gets_the_real_reason(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="not supported on esp8266"):
        bluetooth_proxy.CONFIG_SCHEMA({})


def test_ble_less_platform_connection_keys_fall_through(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The key-level rejection must not fire here — it would imply an
    # advertisement-only proxy exists on this platform.
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="not supported on esp8266"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})


def test_no_target_platform_keeps_the_key_gate_out_of_the_way() -> None:
    # set_core_config cannot express "no platform"; script/build_codeowners.py
    # sets exactly this shape, and the key gate returns early on it so the
    # platform gate is what reports.
    CORE.data[KEY_CORE] = {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: None}
    with pytest.raises(cv.Invalid, match="not supported on None"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 2})


@pytest.mark.parametrize("platform_framework", HUB_PLATFORM_FRAMEWORKS)
def test_hub_platform_rejects_active(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
) -> None:
    set_core_config(platform_framework)
    _register_tracker(platform_framework.value[0])
    with pytest.raises(cv.Invalid, match="Active connections are not supported"):
        bluetooth_proxy.CONFIG_SCHEMA({"active": True})


@pytest.mark.parametrize("platform_framework", HUB_PLATFORM_FRAMEWORKS)
@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("connection_slots", 2),
        ("cache_services", True),
        # Absent from the outer CONFIG_SCHEMA, so this gate is the only test
        # that touches it.
        ("connections", [{}]),
    ],
)
def test_hub_platform_rejects_connection_keys_by_name(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    key: str,
    value: object,
) -> None:
    set_core_config(platform_framework)
    with pytest.raises(cv.Invalid, match=f"'{key}' requires active"):
        bluetooth_proxy.CONFIG_SCHEMA({key: value})


@pytest.mark.parametrize("platform_framework", HUB_PLATFORM_FRAMEWORKS)
def test_hub_platform_accepts_the_advertisement_only_shape(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
) -> None:
    set_core_config(platform_framework)
    _register_tracker(platform_framework.value[0])
    validated = bluetooth_proxy.CONFIG_SCHEMA({})
    assert validated[CONF_ACTIVE] is False


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
