"""LN882H BLE scanner implementing the ble_device_base BLEHub contract on
top of the ln882h_ble controller. With continuous: false nothing scans until
an explicit start_scan() call."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base, ln882h_ble, ota
from esphome.components.ble_device_base import automation as ble_automation
from esphome.components.const import CONF_ON_SCAN_END, CONF_SCAN_PARAMETERS, CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
    CONF_MANUFACTURER_ID,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_SERVICE_UUID,
)
from esphome.core import ID
from esphome.types import ConfigType

CONF_LN882H_BLE_ID = "ln882h_ble_id"

DEPENDENCIES = ["ln882x"]
AUTO_LOAD = ["ble_device_base", "ln882h_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

ble_device_base.register_hub_provider("ln882h_ble_tracker")

ln882h_ble_tracker_ns = cg.esphome_ns.namespace("ln882h_ble_tracker")
LN882HBLETracker = ln882h_ble_tracker_ns.class_(
    "LN882HBLETracker", ble_device_base.BLEHub, cg.Component
)

StartScanAction = ln882h_ble_tracker_ns.class_("StartScanAction", automation.Action)
StopScanAction = ln882h_ble_tracker_ns.class_("StopScanAction", automation.Action)

ESPBTAdvertiseTrigger = ble_automation.ESPBTAdvertiseTrigger
BLEServiceDataAdvertiseTrigger = ble_automation.BLEServiceDataAdvertiseTrigger
BLEManufacturerDataAdvertiseTrigger = ble_automation.BLEManufacturerDataAdvertiseTrigger
BLEEndOfScanTrigger = ble_automation.BLEEndOfScanTrigger


# LN882H SDK reference scan rate: 100 ms interval / 50 ms window (50 % duty).
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema(
    "100ms", window_default="50ms"
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LN882HBLETracker),
        cv.GenerateID(CONF_LN882H_BLE_ID): cv.use_id(ln882h_ble.LN882HBLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
        cv.Optional(CONF_ON_BLE_ADVERTISE): ble_automation.advertise_trigger_schema(
            ESPBTAdvertiseTrigger
        ),
        cv.Optional(
            CONF_ON_BLE_SERVICE_DATA_ADVERTISE
        ): ble_automation.uuid_trigger_schema(
            BLEServiceDataAdvertiseTrigger,
            {cv.Required(CONF_SERVICE_UUID): ble_device_base.bt_uuid},
        ),
        cv.Optional(
            CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE
        ): ble_automation.uuid_trigger_schema(
            BLEManufacturerDataAdvertiseTrigger,
            {cv.Required(CONF_MANUFACTURER_ID): ble_device_base.bt_uuid},
        ),
        cv.Optional(CONF_ON_SCAN_END): ble_automation.scan_end_trigger_schema(
            BLEEndOfScanTrigger
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


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
    automation.maybe_simple_id(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(LN882HBLETracker),
            }
        )
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
    # Selects the BLEHub alias arm in ble_device_base/ble_hub_impl.h.
    cg.add_define("USE_LN882H_BLE_TRACKER")
    # Compiles the shared adv + scan-response merge (the LN controller
    # delivers the pair as separate reports).
    cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")

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
    cg.add(var.set_configured_continuous(scan[CONF_CONTINUOUS]))

    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        await ble_automation.advertise_trigger_to_code(conf, var)

    for trigger_key, uuid_key, setter_prefix in (
        (CONF_ON_BLE_SERVICE_DATA_ADVERTISE, CONF_SERVICE_UUID, "set_service_uuid"),
        (
            CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
            CONF_MANUFACTURER_ID,
            "set_manufacturer_uuid",
        ),
    ):
        for conf in config.get(trigger_key, []):
            await ble_automation.uuid_trigger_to_code(
                conf, var, uuid_key, setter_prefix
            )

    for conf in config.get(CONF_ON_SCAN_END, []):
        await ble_automation.scan_end_trigger_to_code(conf, var)
