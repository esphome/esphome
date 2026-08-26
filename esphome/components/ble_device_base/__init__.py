"""
ble_device_base — the platform-neutral BLE layer.

Owns the shared advertisement types (ESPBTUUID / ESPBTDevice / ServiceData /
ESPBLEiBeacon / ESPBTDeviceListener, in ble_device.h) and the tracker contract
(BLEHub, in ble_hub.h; C++-side a per-platform alias bound in ble_hub_impl.h)
on every platform.

BLE consumers (sensor components, bluetooth_proxy) bind to whichever tracker the
configuration declares via `cv.use_id(BLEHub)` — ESPHome resolves any declared
subclass, so there is no Python platform table here and no dependency in
either direction (C++-side, the compile-time alias header ble_hub_impl.h and the
defines.h mirror are the deliberate exceptions). A sensor extends
BLE_DEVICE_SCHEMA in its CONFIG_SCHEMA (so an explicit ble_hub_id: is a
declared key even on strict schemas) and calls register_ble_device() in
to_code; a tracker component declares BLEHub as its codegen-class parent and
MUST call register_hub_provider() at import time — without it _require_hub
rejects configs that bind through the generated id (an explicit ble_hub_id:
bypasses the registry). Adding a new BLE chip requires a new in-tree tracker
component plus its alias arm and define (see above); out-of-tree BLE hubs
are not supported.

AES-CCM decryption for encrypted advertisements is provided portably in
ble_aes_ccm.h.
"""

from collections.abc import Callable
import re

import esphome.codegen as cg
from esphome.components.const import CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_INTERVAL,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, ID, KEY_CORE, TimePeriod
from esphome.types import ConfigType

CODEOWNERS = ["@Bl00d-B0b"]

CONF_BLE_HUB_ID = "ble_hub_id"

# Number of parsed-advertisement listeners registered in this build; read via
# cg.get_slot_count() by esp32_ble_tracker's feature coupling.
LISTENER_COUNT_DEFINE = "ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT"


ble_device_base_ns = cg.esphome_ns.namespace("ble_device_base")

# The neutral tracker contract. Every tracker's codegen class declares this as
# a parent, which is what lets cv.use_id(BLEHub) resolve any of them. Python
# only: C++-side the name is a per-platform alias (ble_hub_impl.h).
BLEHub = ble_device_base_ns.class_("BLEHub")

# The neutral listener base (C++: ble_device_base::ESPBTDeviceListener).
ESPBTDeviceListener = ble_device_base_ns.class_("ESPBTDeviceListener")


# Config keys that provide a BLEHub, registered by each tracker component at
# import time (a tracker's module is imported iff it can end up in the build).
# Used only to phrase an actionable error when a BLE consumer is configured
# without any tracker — the binding itself resolves any BLEHub subclass and
# needs no platform table. Out-of-tree BLE hubs are not supported; the
# registry and the messages below deal in in-tree trackers only.
_HUB_PROVIDERS: set[str] = set()

# The in-tree trackers per target platform, so the missing-tracker error names
# them even in a fresh process where no tracker module has been imported yet (a
# consumer imports only ble_device_base, so the registry is empty exactly in
# the most common failure: the tracker was simply forgotten). Filtered by the
# current platform so an esp32 config is not told to add a Beken tracker; an
# unknown/absent platform falls back to every in-tree name.
_IN_TREE_HUB_PROVIDERS: dict[str, str] = {
    "esp32": "esp32_ble_tracker",
    "bk72xx": "bk72xx_ble_tracker",
    "rp2": "rp2_ble_tracker",
    "ln882x": "ln882h_ble_tracker",
}


def register_hub_provider(component: str) -> None:
    """Called at import time by every component whose config key declares a BLEHub."""
    _HUB_PROVIDERS.add(component)


def _require_hub(value: ID) -> ID:
    # Without this check a missing tracker surfaces at ID resolution as
    # "Couldn't find any component that can be used for 'ble_device_base::BLEHub'"
    # — a C++ class name the user never types. Component final validation cannot
    # phrase it better: the ID pass runs first and its error skips all later
    # steps. All explicitly configured components are loaded before any schema
    # validates, so a registered provider in loaded_integrations is exact here.
    if value.id is not None:
        # Explicit ble_hub_id: — the user is pointing at a specific hub (the
        # multi-hub disambiguation case). Let the ID pass judge it; its error
        # names the missing id, which is accurate.
        return value
    if not _HUB_PROVIDERS & CORE.loaded_integrations:
        # Defensive lookup rather than CORE.target_platform: the property
        # raises when no platform is registered, and this message must never
        # be the thing that crashes. In a real run the platform is always set
        # (LoadTargetPlatformValidationStep runs before any other domain), so
        # the unfiltered all-platforms fallback is reachable only from tests.
        platform = CORE.data.get(KEY_CORE, {}).get(KEY_TARGET_PLATFORM)
        if platform is not None and platform not in _IN_TREE_HUB_PROVIDERS:
            # Known platform with no in-tree hub (esp8266, host, rtl87xx, …):
            # listing the other platforms' trackers would misdirect, and
            # out-of-tree BLE hubs are not supported.
            raise cv.Invalid(
                f"No BLE tracker exists for {platform}; BLE components are "
                "not supported on this platform"
            )
        in_tree = (
            {tracker}
            if (tracker := _IN_TREE_HUB_PROVIDERS.get(platform))
            else set(_IN_TREE_HUB_PROVIDERS.values())
        )
        # in_tree only: _HUB_PROVIDERS is import-time state that outlives
        # CORE.reset() in a long-lived process (dashboard), so a tracker from
        # an earlier build of another platform must not leak into the message.
        # The gate above is immune — loaded_integrations resets per run.
        names = ", ".join(sorted(in_tree))
        raise cv.Invalid(f"No BLE tracker configured — add one of: {names}")
    return value


# Schema fragment binding a consumer to the configured BLE tracker: extend a
# consumer's CONFIG_SCHEMA with this so ble_hub_id: is a declared key — a
# trailing validator after a PREVENT_EXTRA schema would reject the explicit
# form before ever running. An omitted id resolves to the single declared
# tracker on any platform; multiple trackers are disambiguated with an
# explicit ble_hub_id.
BLE_DEVICE_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_BLE_HUB_ID): cv.All(cv.use_id(BLEHub), _require_hub)}
)


def rename_legacy_hub_id(component: str) -> Callable[[ConfigType], ConfigType]:
    """Transitional alias for the pre-migration binding key: esp32_ble_id ->
    ble_hub_id. Warns and auto-migrates until removal; every migrated platform
    prepends this to its CONFIG_SCHEMA so existing configs keep validating."""
    return cv.rename_key(
        "esp32_ble_id", CONF_BLE_HUB_ID, removed_in="2027.2.0", component=component
    )


def request_irk_support() -> None:
    """Compile in resolve_irk()'s software-AES path. Called by sensors with an
    irk: option so builds without IRK do not carry the resolution code."""
    cg.add_define("USE_BLE_DEVICE_IRK")


# Number of GATT client connection slots in this build; sizes the platform
# backend's connection storage.
GATT_CLIENT_COUNT_DEFINE = "ESPHOME_BLE_GATT_CLIENT_COUNT"

_request_gatt_connection_slot = cg.slot_counter(GATT_CLIENT_COUNT_DEFINE)


def request_gatt_client() -> None:
    """Compile in the neutral GATT client contract (ble_gatt_client.h) and
    claim one compiled-in client slot (sizes ESPHOME_BLE_GATT_CLIENT_COUNT;
    distinct from the proxy's validated connection budget). Called by
    bluetooth_connection.new_gatt_backend() once per backend instance."""
    cg.add_define("USE_BLE_GATT_CLIENT")
    _request_gatt_connection_slot()


_request_listener_slot = cg.slot_counter(LISTENER_COUNT_DEFINE)


async def register_ble_device(var: cg.MockObj, config: ConfigType) -> cg.MockObj:
    """Register `var` as a parsed-advertisement listener on the configured hub."""
    hub = await cg.get_variable(config[CONF_BLE_HUB_ID])
    cg.add(hub.register_listener(var))
    _request_listener_slot()
    return var


# ---- shared validation / codegen helpers (platform-neutral) ----


def to_ble_units(value: cv.TimePeriod) -> int:
    """Convert a scan time to the controller's 0.625 ms units.

    Used by both validation and codegen so what is validated is exactly what is
    programmed — the truncation here is what makes the duty-cycle check below
    meaningful.
    """
    return value.total_microseconds // 625


def validate_scan_parameters(config: ConfigType) -> ConfigType:
    """Reject impossible window/interval/duration combinations at config time.

    The controller cannot scan for longer than the interval, and a too-short
    duration would end the scan period almost immediately. Catching it here
    gives a clear error instead of a runtime controller failure and a retry
    loop.
    """
    duration = config[CONF_DURATION]
    interval = config[CONF_INTERVAL]
    window = config[CONF_WINDOW]

    # Labels are reused in every error below; the optional one names its key.
    windows = [("Scan window", window)]
    if (connection_window := config.get(CONF_CONNECTION_SCAN_WINDOW)) is not None:
        windows.append((CONF_CONNECTION_SCAN_WINDOW, connection_window))

    for name, value in windows:
        if value > interval:
            raise cv.Invalid(
                f"{name} ({value}) needs to be smaller than scan interval ({interval})"
            )

    # BLE scan interval/window are programmed in 0.625 ms units as a 16-bit value; the
    # controller only accepts 2.5 ms .. 10240 ms (0x0004 .. 0x4000). Reject out-of-range
    # values here instead of letting the unit conversion silently overflow.
    for name, value in (("Scan interval", interval), *windows):
        if value.total_microseconds < 2500 or value.total_microseconds > 10_240_000:
            raise cv.Invalid(f"{name} ({value}) must be between 2.5 ms and 10240 ms")

    # Validate what actually reaches the controller: both values are truncated to
    # whole 0.625 ms units, so a window/interval pair that differs by less than one
    # unit collapses to the same value — silently programming a 100 % duty cycle
    # (radio permanently on) from a config that asked for less.
    interval_units = to_ble_units(interval)
    for name, value in windows:
        if to_ble_units(value) == interval_units and value < interval:
            raise cv.Invalid(
                f"{name} ({value}) and interval ({interval}) both truncate to "
                f"{interval_units} x 0.625 ms, which the controller scans at a 100 % duty "
                f"cycle. Separate them by at least 0.625 ms."
            )

    if interval.total_microseconds * 3 > duration.total_microseconds:
        raise cv.Invalid(
            f"Scan duration ({duration}) must cover at least three scan intervals "
            f"({interval}): the scanner listens on one of the three BLE advertising "
            f"channels per interval, so a shorter duration can miss devices entirely."
        )

    return config


# The historical scan window default shared by the trackers that do not pin
# their own; also the fallback for esp32's conditional default.
DEFAULT_SCAN_WINDOW = "30ms"

CONF_CONNECTION_SCAN_WINDOW = "connection_scan_window"


def scan_parameters_schema(
    interval_default: str,
    *,
    window_default: str | Callable[[], TimePeriod] = DEFAULT_SCAN_WINDOW,
    connection_window: bool = False,
) -> cv.All:
    """Build the scan_parameters value schema shared by all BLE trackers.

    interval_default and window_default are per chip (e.g. esp32 320/30 ms,
    bk72xx/rp2 100/30 ms — the reference scan rates of the respective stacks;
    LN882H's SDK recommends 100/50 ms). window_default may also be a zero-arg
    callable evaluated per validation when the user omits the key (esp32 uses
    this to record that the window was defaulted, so a later validation step
    can adjust it once sibling keys are resolved). The `active` option
    (default on) is unconditional: active scanning is part of the tracker
    contract — every current proxy client assumes it, so a passive-only
    tracker must not share this schema. connection_window opts in to the
    `connection_scan_window` option for trackers that can fall back to a
    smaller window while a GATT connection is active.
    """
    schema = {
        cv.Optional(CONF_DURATION, default="5min"): cv.positive_time_period_seconds,
        cv.Optional(CONF_INTERVAL, default=interval_default): cv.positive_time_period,
        cv.Optional(CONF_WINDOW, default=window_default): cv.positive_time_period,
        cv.Optional(CONF_CONTINUOUS, default=True): cv.boolean,
        cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
    }
    if connection_window:
        schema[cv.Optional(CONF_CONNECTION_SCAN_WINDOW)] = cv.positive_time_period
    return cv.All(cv.Schema(schema), validate_scan_parameters)


BT_UUID16_FORMAT = "XXXX"
BT_UUID32_FORMAT = "XXXXXXXX"
BT_UUID128_FORMAT = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"

_BT_UUID16_RE = re.compile("^[A-F0-9]{4,}$")
_BT_UUID32_RE = re.compile("^[A-F0-9]{8,}$")
_BT_UUID128_RE = re.compile(
    "^[A-F0-9]{8,}-[A-F0-9]{4,}-[A-F0-9]{4,}-[A-F0-9]{4,}-[A-F0-9]{12,}$"
)


# Validator table keyed by input length: (compiled pattern, label used in errors).
_BT_UUID_FORMATS = {
    len(BT_UUID16_FORMAT): (_BT_UUID16_RE, "16 bit"),
    len(BT_UUID32_FORMAT): (_BT_UUID32_RE, "32 bit"),
    len(BT_UUID128_FORMAT): (_BT_UUID128_RE, "128"),
}


def bt_uuid(value: str) -> str:
    in_value = cv.string_strict(value)
    value = in_value.upper()

    fmt = _BT_UUID_FORMATS.get(len(value))
    if fmt is None:
        raise cv.Invalid(
            f"Bluetooth UUID must be in 16 bit '{BT_UUID16_FORMAT}', 32 bit '{BT_UUID32_FORMAT}', or 128 bit '{BT_UUID128_FORMAT}' format"
        )
    pattern, label = fmt
    if not pattern.match(value):
        raise cv.Invalid(
            f"Invalid hexadecimal value for {label} UUID format: '{in_value}'"
        )
    return value


def as_hex(value: str) -> cg.RawExpression:
    return cg.RawExpression(f"0x{value}ULL")


def _hex_array_expression(value: str, reverse: bool) -> cg.RawExpression:
    value = value.replace("-", "")
    cpp_array = [
        f"0x{part}" for part in [value[i : i + 2] for i in range(0, len(value), 2)]
    ]
    if reverse:
        cpp_array.reverse()
    return cg.RawExpression(f"(uint8_t*)(const uint8_t[16]){{{','.join(cpp_array)}}}")


def as_hex_array(value: str) -> cg.RawExpression:
    return _hex_array_expression(value, reverse=False)


def as_reversed_hex_array(value: str) -> cg.RawExpression:
    return _hex_array_expression(value, reverse=True)


def add_service_uuid(var: cg.MockObj, service_uuid: str) -> None:
    """Emit the width-matched service-UUID setter for a consumer.

    16-/32-bit UUIDs go out as plain hex literals, 128-bit as a reversed byte
    array (BLE wire order). Shared here so every sensor platform dispatches the
    same way instead of carrying its own if/elif copy.
    """
    if len(service_uuid) == len(BT_UUID16_FORMAT):
        cg.add(var.set_service_uuid16(as_hex(service_uuid)))
    elif len(service_uuid) == len(BT_UUID32_FORMAT):
        cg.add(var.set_service_uuid32(as_hex(service_uuid)))
    elif len(service_uuid) == len(BT_UUID128_FORMAT):
        cg.add(var.set_service_uuid128(as_reversed_hex_array(service_uuid)))
    else:
        # bt_uuid restricts lengths to exactly these three formats; if that
        # ever loosens, fail the build instead of emitting no setter (a
        # sensor whose match_by_ is unset silently never matches). ValueError,
        # not cv.Invalid: this runs from to_code, after validation, where
        # voluptuous errors surface as raw tracebacks.
        raise ValueError(f"Unsupported UUID format: {service_uuid}")
