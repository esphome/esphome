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
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@Bl00d-B0b"]

CONF_BLE_HUB_ID = "ble_hub_id"

# CORE.data key: number of parsed-advertisement listeners registered in this
# build. Trackers whose codegen sizes storage at compile time (esp32's
# StaticVector count define) read it in their final coroutine.
KEY_BLE_LISTENER_COUNT = "ble_device_base_listener_count"

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


def get_listener_count() -> int:
    """Number of parsed listeners registered so far (for tracker codegen)."""
    return CORE.data.get(KEY_BLE_LISTENER_COUNT, 0)


async def register_ble_device(var: cg.MockObj, config: ConfigType) -> cg.MockObj:
    """Register `var` as a parsed-advertisement listener on the configured hub."""
    hub = await cg.get_variable(config[CONF_BLE_HUB_ID])
    cg.add(hub.register_listener(var))
    CORE.data[KEY_BLE_LISTENER_COUNT] = CORE.data.get(KEY_BLE_LISTENER_COUNT, 0) + 1
    return var


# ---- shared validation / codegen helpers (platform-neutral) ----
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
