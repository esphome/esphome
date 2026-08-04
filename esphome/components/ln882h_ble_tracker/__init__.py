"""LN882H BLE scanner implementing the ble_device_base BLEHub contract on
top of the ln882h_ble controller. With continuous: false nothing scans until
an explicit start_scan() call."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base, ln882h_ble, ota
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
    CONF_MAC_ADDRESS,
    CONF_MANUFACTURER_ID,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
)
from esphome.core import ID
from esphome.types import ConfigType

CONF_LN882H_BLE_ID = "ln882h_ble_id"
CONF_ON_SCAN_END = "on_scan_end"

DEPENDENCIES = ["ln882x"]
AUTO_LOAD = ["ble_device_base", "ln882h_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

ln882h_ble_tracker_ns = cg.esphome_ns.namespace("ln882h_ble_tracker")
LN882HBLETracker = ln882h_ble_tracker_ns.class_(
    "LN882HBLETracker", ble_device_base.BLEHub, cg.Component
)

StartScanAction = ln882h_ble_tracker_ns.class_("StartScanAction", automation.Action)
StopScanAction = ln882h_ble_tracker_ns.class_("StopScanAction", automation.Action)

ESPBTDeviceConstRef = (
    cg.esphome_ns.namespace("ble_device_base")
    .class_("ESPBTDevice")
    .operator("ref")
    .operator("const")
)
ESPBTAdvertiseTrigger = ln882h_ble_tracker_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")
BLEServiceDataAdvertiseTrigger = ln882h_ble_tracker_ns.class_(
    "BLEServiceDataAdvertiseTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEManufacturerDataAdvertiseTrigger = ln882h_ble_tracker_ns.class_(
    "BLEManufacturerDataAdvertiseTrigger",
    automation.Trigger.template(adv_data_t_const_ref),
)
BLEEndOfScanTrigger = ln882h_ble_tracker_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)


# LN882H SDK reference scan rate: 100 ms interval / 50 ms window (50 % duty).
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema(
    "100ms", window_default="50ms", supports_active=True
)


# UUID string length -> setter width. 16/32-bit go out as plain hex literals,
# 128-bit as a reversed byte array (BLE wire order). Keyed exhaustively so an
# impossible length fails as a KeyError instead of silently picking a width
# (bt_uuid validation upstream only ever produces these three).
_UUID_WIDTHS = {
    len(ble_device_base.BT_UUID16_FORMAT): "16",
    len(ble_device_base.BT_UUID32_FORMAT): "32",
    len(ble_device_base.BT_UUID128_FORMAT): "128",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LN882HBLETracker),
        cv.GenerateID(CONF_LN882H_BLE_ID): cv.use_id(ln882h_ble.LN882HBLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
        cv.Optional(CONF_ON_BLE_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPBTAdvertiseTrigger),
                cv.Optional(CONF_MAC_ADDRESS): cv.ensure_list(cv.mac_address),
            }
        ),
        cv.Optional(CONF_ON_BLE_SERVICE_DATA_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEServiceDataAdvertiseTrigger
                ),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Required(CONF_SERVICE_UUID): ble_device_base.bt_uuid,
            }
        ),
        cv.Optional(
            CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE
        ): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEManufacturerDataAdvertiseTrigger
                ),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Required(CONF_MANUFACTURER_ID): ble_device_base.bt_uuid,
            }
        ),
        cv.Optional(CONF_ON_SCAN_END): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEEndOfScanTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


# Triggers register as ble_device_base listeners in their constructors; count
# them where they are created so the StaticVector cannot be undersized. Shares
# the define with register_ble_device() via the core slot-counter factory.
_count_listener = cg.slot_counter(ble_device_base.LISTENER_COUNT_DEFINE)


@automation.register_action(
    "ln882h_ble_tracker.start_scan",
    StartScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(LN882HBLETracker),
            cv.Optional(CONF_CONTINUOUS): cv.templatable(cv.boolean),
        }
    ),
    synchronous=True,
)
async def start_scan_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: list,
) -> cg.MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if (continuous := config.get(CONF_CONTINUOUS)) is not None:
        template_ = await cg.templatable(continuous, args, cg.bool_)
        cg.add(var.set_continuous(template_))
    return var


@automation.register_action(
    "ln882h_ble_tracker.stop_scan",
    StopScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(LN882HBLETracker),
        }
    ),
    synchronous=True,
)
async def stop_scan_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: list,
) -> cg.MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_LN882H_BLE_ID])
    cg.add(var.set_parent(parent))
    # The tracker registers itself as a controller scan listener in setup();
    # request the codegen-sized StaticVector slot for it.
    ln882h_ble.request_scan_listener_slot()

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_active(scan[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(scan[CONF_CONTINUOUS]))

    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_MAC_ADDRESS in conf:
            addr_list = [it.as_hex for it in conf[CONF_MAC_ADDRESS]]
            cg.add(trigger.set_addresses(addr_list))
        await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
        _count_listener()

    for trigger_key, uuid_key, setter_prefix in (
        (CONF_ON_BLE_SERVICE_DATA_ADVERTISE, CONF_SERVICE_UUID, "set_service_uuid"),
        (
            CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
            CONF_MANUFACTURER_ID,
            "set_manufacturer_uuid",
        ),
    ):
        for conf in config.get(trigger_key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            uuid = conf[uuid_key]
            width = _UUID_WIDTHS[len(uuid)]
            value = (
                ble_device_base.as_hex(uuid)
                if width != "128"
                else ble_device_base.as_reversed_hex_array(uuid)
            )
            cg.add(getattr(trigger, f"{setter_prefix}{width}")(value))
            if CONF_MAC_ADDRESS in conf:
                cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
            await automation.build_automation(
                trigger, [(adv_data_t_const_ref, "x")], conf
            )
            _count_listener()

    for conf in config.get(CONF_ON_SCAN_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        _count_listener()
