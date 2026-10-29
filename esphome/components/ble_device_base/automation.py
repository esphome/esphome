"""Shared codegen for the neutral BLE advertisement triggers (automation.h)."""

from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_MANUFACTURER_ID,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
)
from esphome.cpp_generator import LambdaExpression, MockObjClass
from esphome.types import ConfigType

from . import (
    BT_UUID16_FORMAT,
    BT_UUID32_FORMAT,
    BT_UUID128_FORMAT,
    LISTENER_COUNT_DEFINE,
    ble_device_base_ns,
    bt_uuid,
    hex_bytes,
)

adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")
ESPBTDeviceConstRef = (
    ble_device_base_ns.class_("ESPBTDevice").operator("ref").operator("const")
)

ESPBTAdvertiseTrigger = ble_device_base_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
BLEAdvertiseDataTrigger = ble_device_base_ns.class_(
    "BLEAdvertiseDataTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEEndOfScanTrigger = ble_device_base_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)

# UUID string length -> constexpr ESPBTUUID factory. 16/32-bit take a hex literal,
# 128-bit a reversed byte array (BLE wire order). Keyed exhaustively so an
# impossible length fails as a KeyError instead of silently picking a width
# (bt_uuid validation upstream only ever produces these three).
_UUID_FACTORIES = {
    len(BT_UUID16_FORMAT): "from_uint16",
    len(BT_UUID32_FORMAT): "from_uint32",
    len(BT_UUID128_FORMAT): "from_raw",
}


def _uuid_trigger_schema(uuid_key: str) -> Callable[[Any], Any]:
    """Schema for a UUID-filtered trigger — pairs with _uuid_trigger_to_code()."""
    return automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEAdvertiseDataTrigger),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Required(uuid_key): bt_uuid,
        }
    )


def _filter_mac(value: Any) -> Any:
    mac = cv.mac_address(value)
    if not any(mac.parts):
        # 0 ends the flash MAC table, and it is never a real device address.
        raise cv.Invalid("00:00:00:00:00:00 cannot be used as a MAC address filter")
    return mac


MAC_FILTER_LIST = cv.ensure_list(_filter_mac)


def _advertise_trigger_schema() -> Callable[[Any], Any]:
    """on_ble_advertise schema: multi-mac list filter, unlike the single-mac
    _uuid_trigger_schema() — pairs with _advertise_trigger_to_code()."""
    return automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPBTAdvertiseTrigger),
            cv.Optional(CONF_MAC_ADDRESS): MAC_FILTER_LIST,
        }
    )


def scan_end_trigger_schema(trigger_class: MockObjClass) -> Callable[[Any], Any]:
    """on_scan_end schema: id only — pairs with scan_end_trigger_to_code()."""
    return automation.validate_automation(
        {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_class)}
    )


# Triggers register as ble_device_base listeners in their constructors; count
# them where they are created so no backend can undercount the StaticVector
# (push_back past capacity drops silently). Shares the define with
# register_ble_device() via the core slot-counter factory.
_count_listener = cg.slot_counter(LISTENER_COUNT_DEFINE)


def mac_filter_table(macs: list) -> cg.MockObj:
    """Shared flash table of MACs ended by 0 (never a valid address), so triggers store a pointer."""
    return cg.shared_progmem_array(
        "ble_mac_filter", cg.uint64, [*(mac.as_hex for mac in macs), 0]
    )


async def _advertise_trigger_to_code(conf: ConfigType, var: cg.MockObj) -> None:
    """Build an on_ble_advertise trigger (optional multi-mac filter)."""
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    if macs := conf.get(CONF_MAC_ADDRESS):
        cg.add(trigger.set_addresses(mac_filter_table(macs)))
    await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
    _count_listener()


async def scan_end_trigger_to_code(conf: ConfigType, var: cg.MockObj) -> None:
    """Build an on_scan_end trigger."""
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    await automation.build_automation(trigger, [], conf)
    _count_listener()


def _uuid_match_lambda(uuid: str, getter: str, mac: Any) -> LambdaExpression:
    """Match function for a UUID-filtered trigger: optional MAC test, then the
    first entry of `device.<getter>()` with this UUID (a constexpr, so in flash)."""
    factory = _UUID_FACTORIES[len(uuid)]
    lines = []
    if mac is not None:
        lines.append(f"if (device.address_uint64() != {mac.as_hex}) return nullptr;")
    if factory == "from_raw":
        lines.append(
            f"static constexpr uint8_t RAW[16] = {{{', '.join(hex_bytes(uuid, True))}}};"
        )
        arg = "RAW"
    else:
        arg = f"0x{uuid}"
    lines.append(
        f"static constexpr ble_device_base::ESPBTUUID UUID = ble_device_base::ESPBTUUID::{factory}({arg});"
    )
    lines.append(f"return ble_device_base::find_adv_data(device.{getter}(), UUID);")
    return LambdaExpression(
        "\n".join(lines),
        [(ESPBTDeviceConstRef, "device")],
        capture="",
        return_type=adv_data_t.operator("ptr").operator("const"),
    )


async def _uuid_trigger_to_code(
    conf: ConfigType, var: cg.MockObj, key: str, getter: str
) -> None:
    """Build a UUID-filtered advertise trigger.

    `getter` names the ESPBTDevice data list to search (get_service_datas or
    get_manufacturer_datas); the UUID and MAC filter live in the generated match function.
    """
    match = _uuid_match_lambda(conf[key], getter, conf.get(CONF_MAC_ADDRESS))
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var, match)
    await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    _count_listener()


# (trigger key, uuid key, ESPBTDevice data getter) of the UUID-filtered advertise triggers.
_UUID_TRIGGERS = (
    (CONF_ON_BLE_SERVICE_DATA_ADVERTISE, CONF_SERVICE_UUID, "get_service_datas"),
    (
        CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
        CONF_MANUFACTURER_ID,
        "get_manufacturer_datas",
    ),
)

# Advertise trigger keys shared by every tracker; pairs with advertise_triggers_to_code().
ADVERTISE_TRIGGERS_SCHEMA = {
    cv.Optional(CONF_ON_BLE_ADVERTISE): _advertise_trigger_schema(),
    cv.Optional(CONF_ON_BLE_SERVICE_DATA_ADVERTISE): _uuid_trigger_schema(
        CONF_SERVICE_UUID
    ),
    cv.Optional(CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE): _uuid_trigger_schema(
        CONF_MANUFACTURER_ID
    ),
}


async def advertise_triggers_to_code(config: ConfigType, var: cg.MockObj) -> None:
    """Build every ADVERTISE_TRIGGERS_SCHEMA trigger in a tracker config on hub `var`."""
    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        await _advertise_trigger_to_code(conf, var)
    for trigger_key, uuid_key, getter in _UUID_TRIGGERS:
        for conf in config.get(trigger_key, []):
            await _uuid_trigger_to_code(conf, var, uuid_key, getter)
