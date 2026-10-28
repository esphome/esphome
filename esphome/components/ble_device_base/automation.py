"""Shared codegen for the neutral BLE advertisement triggers (automation.h)."""

from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MAC_ADDRESS, CONF_TRIGGER_ID
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

from . import (
    BT_UUID16_FORMAT,
    BT_UUID32_FORMAT,
    BT_UUID128_FORMAT,
    LISTENER_COUNT_DEFINE,
    ble_device_base_ns,
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
# One class serves both keys; the generated match function picks the data list.
BLEServiceDataAdvertiseTrigger = BLEAdvertiseDataTrigger
BLEManufacturerDataAdvertiseTrigger = BLEAdvertiseDataTrigger
BLEEndOfScanTrigger = ble_device_base_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)

# UUID string length -> ESPBTUUID width. 16/32-bit go out as plain hex literals,
# 128-bit as a reversed byte array (BLE wire order). Keyed exhaustively so an
# impossible length fails as a KeyError instead of silently picking a width
# (bt_uuid validation upstream only ever produces these three).
_UUID_WIDTHS = {
    len(BT_UUID16_FORMAT): "16",
    len(BT_UUID32_FORMAT): "32",
    len(BT_UUID128_FORMAT): "128",
}


def uuid_trigger_schema(
    trigger_class: MockObjClass, extra: dict[Any, Any] | None = None
) -> Callable[[Any], Any]:
    """Schema for a UUID-filtered trigger — pairs with uuid_trigger_to_code().

    `extra` carries the required UUID key (a cv marker, so a dict rather than
    **kwargs); the optional single-mac filter is what uuid_trigger_to_code()
    reads back.
    """
    return automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_class),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            **(extra or {}),
        }
    )


def _filter_mac(value: Any) -> Any:
    mac = cv.mac_address(value)
    if not any(mac.parts):
        # 0 ends the flash MAC table, and it is never a real device address.
        raise cv.Invalid("00:00:00:00:00:00 cannot be used as a MAC address filter")
    return mac


MAC_FILTER_LIST = cv.ensure_list(_filter_mac)


def advertise_trigger_schema(trigger_class: MockObjClass) -> Callable[[Any], Any]:
    """on_ble_advertise schema: multi-mac list filter, unlike the single-mac
    uuid_trigger_schema() — pairs with advertise_trigger_to_code()."""
    return automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_class),
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


async def advertise_trigger_to_code(conf: ConfigType, var: cg.MockObj) -> None:
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


def _uuid_match_lambda(uuid: str, getter: str, mac: Any) -> cg.RawExpression:
    """Match function for a UUID-filtered trigger: optional MAC test, then the
    first entry of `device.<getter>()` with this UUID."""
    body = []
    if mac is not None:
        body.append(f"if (device.address_uint64() != {mac.as_hex}) return nullptr;")
    width = _UUID_WIDTHS[len(uuid)]
    if width == "128":
        raw = uuid.replace("-", "")
        wire = ", ".join(f"0x{raw[i : i + 2]}" for i in range(len(raw) - 2, -1, -2))
        body.append(f"static constexpr uint8_t UUID[16] = {{{wire}}};")
        uuid_expr = "ble_device_base::ESPBTUUID::from_raw(UUID)"
    else:
        uuid_expr = f"ble_device_base::ESPBTUUID::from_uint{width}(0x{uuid})"
    body.append(
        f"return ble_device_base::find_adv_data(device.{getter}(), {uuid_expr});"
    )
    return cg.RawExpression(
        "[](const ble_device_base::ESPBTDevice &device) -> const ble_device_base::adv_data_t * { "
        + " ".join(body)
        + " }"
    )


async def uuid_trigger_to_code(
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
