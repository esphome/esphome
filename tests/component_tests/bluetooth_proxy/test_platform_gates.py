"""The three platform-gate branches: BLE-less platforms are rejected with the
real reason, hub platforms reject GATT-only options by name, and the
advertisement-only arm applies its own defaults."""

from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components import ble_device_base, bluetooth_connection, bluetooth_proxy
from esphome.config_helpers import frameworks_for_platforms
from esphome.const import (
    CONF_ACTIVE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PlatformFramework,
)
from esphome.core import CORE

from ..types import SetCoreConfigCallable

# Advertisement-only hub platforms; rp2 runs the full proxy and has its own
# tests below.
HUB_PLATFORM_FRAMEWORKS = [
    PlatformFramework.BK72XX_ARDUINO,
    PlatformFramework.LN882X_ARDUINO,
]

HUB_TRACKERS = {
    PLATFORM_BK72XX: "bk72xx_ble_tracker",
    PLATFORM_LN882X: "ln882h_ble_tracker",
    PLATFORM_RP2: "rp2_ble_tracker",
}


def test_hub_platform_list_covers_every_hub_platform() -> None:
    # A platform added to _HUB_PLATFORMS would otherwise get no gate coverage
    # at all; GATT platforms have their own tests.
    advertisement_only = set(bluetooth_proxy._HUB_PLATFORMS) - set(
        bluetooth_connection.HUB_MAX_CONNECTIONS
    )
    assert {pf.value[0] for pf in HUB_PLATFORM_FRAMEWORKS} == advertisement_only
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


def test_rp2_defaults_to_the_full_proxy(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # esp32 parity: active defaults to true, with the platform's slot limit,
    # and one populated connection entry for the codegen to index.
    set_core_config(PlatformFramework.RP2_ARDUINO)
    _register_tracker(PLATFORM_RP2)
    validated = bluetooth_proxy.CONFIG_SCHEMA({})
    assert validated[CONF_ACTIVE] is True
    assert validated[bluetooth_proxy.CONF_CONNECTION_SLOTS] == 3
    assert len(validated[bluetooth_proxy.CONF_CONNECTIONS]) == 3


def test_rp2_accepts_explicit_passive(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.RP2_ARDUINO)
    _register_tracker(PLATFORM_RP2)
    validated = bluetooth_proxy.CONFIG_SCHEMA({CONF_ACTIVE: False})
    assert validated[CONF_ACTIVE] is False
    assert bluetooth_proxy.CONF_CONNECTIONS not in validated


def test_rp2_rejects_slots_beyond_the_btstack_limit(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The BTstack pool overrides are sized for RP2_MAX_CONNECTIONS slots.
    set_core_config(PlatformFramework.RP2_ARDUINO)
    _register_tracker(PLATFORM_RP2)
    with pytest.raises(cv.Invalid, match="at most 3 connection slot"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 4})
    # Fewer slots than the cap stay accepted (the prebuilt single-client pool
    # path for 1, the wrap path for 2).
    validated = bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 1})
    assert len(validated[bluetooth_proxy.CONF_CONNECTIONS]) == 1
    # Values past even the loosest platform cap stop at the outer walkable
    # schema, which stays bounded for range walkers (device-builder sync);
    # in-range values get the platform message above.
    with pytest.raises(cv.Invalid, match="at most 9"):
        bluetooth_proxy.CONFIG_SCHEMA({"connection_slots": 12})


def test_rp2_rejects_esp32_only_keys_by_name(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.RP2_ARDUINO)
    _register_tracker(PLATFORM_RP2)
    with pytest.raises(cv.Invalid, match="'cache_services' is esp32-only"):
        bluetooth_proxy.CONFIG_SCHEMA({"cache_services": True})
    with pytest.raises(cv.Invalid, match="'connections' has no per-connection options"):
        bluetooth_proxy.CONFIG_SCHEMA({"connections": [{}]})


def test_hub_source_filter_covers_every_hub_platform() -> None:
    # bluetooth_connection cannot import this module to derive the hub.cpp
    # framework set, so pin it here: a platform admitted to the proxy but
    # missing from the filter would validate, then fail at link.
    expected = frameworks_for_platforms(
        [*bluetooth_proxy._HUB_PLATFORMS, PLATFORM_ESP32]
    )
    hub_frameworks = bluetooth_connection.SOURCE_FILE_FRAMEWORKS[
        "bluetooth_connection_hub.cpp"
    ]
    assert expected == hub_frameworks


def test_bluetooth_connection_auto_load_covers_its_includes() -> None:
    # The backend registers with its platform BLE stack (and the Bluedroid
    # header includes the tracker's), so that closure lives here and
    # consumers stay platform-blind; the platform-less arm is the union for
    # manifest-resolving tooling.
    _set_platform("esp32")
    assert bluetooth_connection.AUTO_LOAD() == ["ble_device_base", "esp32_ble_tracker"]
    _set_platform("rp2")
    assert bluetooth_connection.AUTO_LOAD() == ["ble_device_base", "rp2040_ble"]
    _set_platform("ln882x")
    assert bluetooth_connection.AUTO_LOAD() == ["ble_device_base"]
    _set_platform(None)
    assert bluetooth_connection.AUTO_LOAD() == [
        "ble_device_base",
        "esp32_ble_tracker",
        "rp2040_ble",
    ]


def test_every_registered_hub_platform_has_a_schema_arm() -> None:
    # A platform added to HUB_MAX_CONNECTIONS without a schema builder or
    # _HUB_PLATFORMS entry would only fail when a config for it is validated
    # (or not even then); pin both couplings here. Connection codegen is
    # shared (bluetooth_connection.new_gatt_backend), so it needs no arm.
    registered = set(bluetooth_connection.HUB_MAX_CONNECTIONS)
    assert registered <= set(bluetooth_proxy._GATT_HUB_SCHEMAS)
    assert registered <= set(bluetooth_proxy._HUB_PLATFORMS)
    # Hub platforms must also be in the backend registry the shared codegen
    # helpers dispatch on.
    assert registered <= set(bluetooth_connection._PLATFORM_BACKENDS)
    # The outer walkable schema's bound must stay the loosest platform cap.
    assert (
        max(bluetooth_connection.HUB_MAX_CONNECTIONS.values())
        <= bluetooth_proxy._IDF_MAX_CONNECTIONS
    )


def test_defines_h_mirrors_the_rp2_slot_cap() -> None:
    # esphome/core/defines.h carries a literal BLUETOOTH_PROXY_MAX_CONNECTIONS
    # for static analysis; pin it to the real rp2 cap.
    defines = (Path(__file__).parents[3] / "esphome" / "core" / "defines.h").read_text()
    cap = bluetooth_connection.RP2_MAX_CONNECTIONS
    # The rp2 arm's define, tolerating blank/comment lines in between.
    match = re.search(
        r"#elif defined\(USE_RP2\)\s*(?:(?://[^\n]*)?\n)+#define BLUETOOTH_PROXY_MAX_CONNECTIONS (\d+)",
        defines,
    )
    assert match is not None, "no USE_RP2 arm defines BLUETOOTH_PROXY_MAX_CONNECTIONS"
    assert int(match.group(1)) == cap, (
        f"defines.h rp2 arm carries {match.group(1)}, expected {cap}"
    )
    # The static-analysis client count scales with the same cap. Scoped to
    # the USE_RP2 block: the esp32 arm carries its own count.
    rp2_block = re.search(r"#ifdef USE_RP2\n((?:#define [^\n]*\n)+)", defines)
    assert rp2_block is not None, "no USE_RP2 platform block in defines.h"
    match = re.search(
        r"#define ESPHOME_BLE_GATT_CLIENT_COUNT (\d+)", rp2_block.group(1)
    )
    assert match is not None, "ESPHOME_BLE_GATT_CLIENT_COUNT missing from rp2 block"
    assert int(match.group(1)) == cap, (
        f"rp2 ESPHOME_BLE_GATT_CLIENT_COUNT is {match.group(1)}, expected {cap}"
    )
