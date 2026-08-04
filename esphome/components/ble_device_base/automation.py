"""Shared codegen for the neutral BLE advertisement triggers (automation.h)."""

from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_MAC_ADDRESS, CONF_TRIGGER_ID
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")
ESPBTDeviceConstRef = (
    ble_device_base.ble_device_base_ns.class_("ESPBTDevice")
    .operator("ref")
    .operator("const")
)

ESPBTAdvertiseTrigger = ble_device_base.ble_device_base_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
BLEServiceDataAdvertiseTrigger = ble_device_base.ble_device_base_ns.class_(
    "BLEServiceDataAdvertiseTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEManufacturerDataAdvertiseTrigger = ble_device_base.ble_device_base_ns.class_(
    "BLEManufacturerDataAdvertiseTrigger",
    automation.Trigger.template(adv_data_t_const_ref),
)
BLEEndOfScanTrigger = ble_device_base.ble_device_base_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)


def trigger_schema(
    trigger_class: MockObjClass, extra: dict[Any, Any] | None = None
) -> cv.Schema:
    """Automation schema for a tracker trigger: id, optional mac filter, extras.

    `extra` is a dict rather than **kwargs because schema keys are cv markers,
    not strings.
    """
    return automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_class),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            **(extra or {}),
        }
    )


# Triggers register as ble_device_base listeners in their constructors; count
# them where they are created so no backend can undercount the StaticVector
# (push_back past capacity drops silently). Shares the define with
# register_ble_device() via the core slot-counter factory.
_count_listener = cg.slot_counter(ble_device_base.LISTENER_COUNT_DEFINE)


async def advertise_trigger_to_code(conf: ConfigType, var: cg.MockObj) -> None:
    """Build an on_ble_advertise trigger (optional multi-mac filter)."""
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    if (macs := conf.get(CONF_MAC_ADDRESS)) is not None:
        cg.add(trigger.set_addresses([it.as_hex for it in macs]))
    await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
    _count_listener()


async def scan_end_trigger_to_code(conf: ConfigType, var: cg.MockObj) -> None:
    """Build an on_scan_end trigger."""
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    await automation.build_automation(trigger, [], conf)
    _count_listener()


async def uuid_trigger_to_code(
    conf: ConfigType, var: cg.MockObj, key: str, setter_prefix: str
) -> None:
    """Build a UUID-filtered advertise trigger.

    The UUID width picks the setter: 16-/32-bit go out as a plain hex literal,
    128-bit as a reversed byte array (BLE wire order).
    """
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
    uuid = conf[key]
    if len(uuid) == len(ble_device_base.BT_UUID16_FORMAT):
        setter, value = f"{setter_prefix}16", ble_device_base.as_hex(uuid)
    elif len(uuid) == len(ble_device_base.BT_UUID32_FORMAT):
        setter, value = f"{setter_prefix}32", ble_device_base.as_hex(uuid)
    else:
        setter, value = (
            f"{setter_prefix}128",
            ble_device_base.as_reversed_hex_array(uuid),
        )
    cg.add(getattr(trigger, setter)(value))
    if (mac := conf.get(CONF_MAC_ADDRESS)) is not None:
        cg.add(trigger.set_address(mac.as_hex))
    await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    _count_listener()
