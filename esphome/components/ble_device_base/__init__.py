"""
ble_device_base — the platform-neutral BLE layer.

Owns the shared advertisement types (ESPBTUUID / ESPBTDevice / ServiceData /
ESPBLEiBeacon / ESPBTDeviceListener, in ble_device.h) and the tracker contract
(BLEHub, in ble_hub.h) on every platform.

BLE consumers (sensor components, bluetooth_proxy) bind to whichever tracker the
configuration declares via `cv.use_id(BLEHub)` — ESPHome resolves any declared
subclass, so there is no platform table here and no dependency in either
direction. A sensor appends inject_ble_hub to its CONFIG_SCHEMA (via cv.All) and
calls register_ble_device() in to_code; a tracker component subclasses BLEHub
(C++ and codegen class). Adding a new BLE chip requires only a new tracker
component.

AES-CCM decryption for encrypted advertisements is provided portably in
ble_aes_ccm.h.
"""

import re

import esphome.codegen as cg
from esphome.components.const import CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_CONTINUOUS, CONF_DURATION, CONF_INTERVAL
from esphome.types import ConfigType

CODEOWNERS = ["@Bl00d-B0b"]

CONF_BLE_HUB_ID = "ble_hub_id"

# Number of parsed-advertisement listeners registered in this build; read via
# cg.get_slot_count() by esp32_ble_tracker's feature coupling.
LISTENER_COUNT_DEFINE = "ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT"


ble_device_base_ns = cg.esphome_ns.namespace("ble_device_base")

# The neutral tracker contract. Every tracker's codegen class declares this as a
# parent, which is what lets cv.use_id(BLEHub) resolve any of them.
BLEHub = ble_device_base_ns.class_("BLEHub")

# The neutral listener base (C++: ble_device_base::ESPBTDeviceListener).
ESPBTDeviceListener = ble_device_base_ns.class_("ESPBTDeviceListener")


def inject_ble_hub(config: ConfigType) -> ConfigType:
    """Validator: auto-resolve the configured BLE tracker into the config.

    Append via cv.All to a BLE consumer's CONFIG_SCHEMA. Uses cv.GenerateID +
    cv.use_id(BLEHub): an omitted id resolves to the single declared tracker on
    any platform; multiple trackers can be disambiguated with an explicit
    ble_hub_id.
    """
    return cv.Schema(
        {cv.GenerateID(CONF_BLE_HUB_ID): cv.use_id(BLEHub)}, extra=cv.ALLOW_EXTRA
    )(config)


def request_irk_support() -> None:
    """Compile in resolve_irk()'s software-AES path. Called by sensors with an
    irk: option so builds without IRK do not carry the resolution code."""
    cg.add_define("USE_BLE_DEVICE_IRK")


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

    if window > interval:
        raise cv.Invalid(
            f"Scan window ({window}) needs to be smaller than scan interval ({interval})"
        )

    # BLE scan interval/window are programmed in 0.625 ms units as a 16-bit value; the
    # controller only accepts 2.5 ms .. 10240 ms (0x0004 .. 0x4000). Reject out-of-range
    # values here instead of letting the unit conversion silently overflow.
    for name, value in (("interval", interval), ("window", window)):
        if value.total_microseconds < 2500 or value.total_microseconds > 10_240_000:
            raise cv.Invalid(
                f"Scan {name} ({value}) must be between 2.5 ms and 10240 ms"
            )

    # Validate what actually reaches the controller: both values are truncated to
    # whole 0.625 ms units, so a window/interval pair that differs by less than one
    # unit collapses to the same value — silently programming a 100 % duty cycle
    # (radio permanently on) from a config that asked for less.
    interval_units = to_ble_units(interval)
    window_units = to_ble_units(window)
    if window_units == interval_units and window < interval:
        raise cv.Invalid(
            f"Scan window ({window}) and interval ({interval}) both truncate to "
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


def scan_parameters_schema(
    interval_default: str,
    *,
    window_default: str = "30ms",
    supports_active: bool = False,
) -> cv.All:
    """Build the scan_parameters value schema shared by all BLE trackers.

    interval_default and window_default are per chip (e.g. esp32 320/30 ms,
    bk72xx/rp2 100/30 ms — the reference scan rates of the respective stacks;
    LN882H's SDK recommends 100/50 ms). Pass supports_active=True only when
    the tracker supports active scanning; it exposes the `active` option
    (whose own default is on, esp32_ble_tracker behavior).
    """
    schema = {
        cv.Optional(CONF_DURATION, default="5min"): cv.positive_time_period_seconds,
        cv.Optional(CONF_INTERVAL, default=interval_default): cv.positive_time_period,
        cv.Optional(CONF_WINDOW, default=window_default): cv.positive_time_period,
        cv.Optional(CONF_CONTINUOUS, default=True): cv.boolean,
    }
    if supports_active:
        schema[cv.Optional(CONF_ACTIVE, default=True)] = cv.boolean
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
