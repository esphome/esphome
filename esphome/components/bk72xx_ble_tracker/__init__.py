"""BK72xx BLE Tracker — ESPHome BLE 5.x scanner for the BLE-5.x-capable
LibreTiny Beken chips (beken-72xx family).

Builds on the bk72xx_ble controller component (stack bring-up, BLE address,
scan primitives) and implements the platform-neutral ble_device_base BLEHub
contract: the shared BLE sensors (ble_presence, ble_rssi, ble_scanner,
bthome_mithermometer, xiaomi_*, …) bind to this tracker through
cv.use_id(BLEHub) with no BK-specific code.

Scan modes:
  continuous: true  — scan runs forever; never stops automatically.
                      Use this when the radio is dedicated to BLE.
  continuous: false — a started scan runs for `duration` ms, then stops. The
                      FIRST start is external too: nothing in this component
                      starts a non-continuous scan on boot — the radio stays
                      idle until bk72xx_ble_tracker.start_scan fires (e.g.
                      from an api client-connected automation), so the
                      single-core radio can service WiFi in between scans.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import bk72xx_ble, ble_device_base, ota
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

CONF_BK72XX_BLE_ID = "bk72xx_ble_id"

DEPENDENCIES = ["bk72xx"]
AUTO_LOAD = ["ble_device_base", "bk72xx_ble"]
CODEOWNERS = ["@Bl00d-B0b"]

ble_device_base.register_hub_provider("bk72xx_ble_tracker")

bk72xx_ble_tracker_ns = cg.esphome_ns.namespace("bk72xx_ble_tracker")
BK72xxBLETracker = bk72xx_ble_tracker_ns.class_(
    "BK72xxBLETracker", ble_device_base.BLEHub, cg.Component
)

StartScanAction = bk72xx_ble_tracker_ns.class_("StartScanAction", automation.Action)
StopScanAction = bk72xx_ble_tracker_ns.class_("StopScanAction", automation.Action)

ESPBTAdvertiseTrigger = ble_automation.ESPBTAdvertiseTrigger
BLEServiceDataAdvertiseTrigger = ble_automation.BLEServiceDataAdvertiseTrigger
BLEManufacturerDataAdvertiseTrigger = ble_automation.BLEManufacturerDataAdvertiseTrigger
BLEEndOfScanTrigger = ble_automation.BLEEndOfScanTrigger


# interval defaults to the BK reference scan rate — 100 ms with the shared 30 ms
# window, a 30 % duty cycle. Converted to the controller's 0.625 ms BLE units in
# to_code(). (LN882H's SDK recommends a different 100 / 50 ms = 50 %.)
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema("100ms")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BK72xxBLETracker),
        cv.GenerateID(CONF_BK72XX_BLE_ID): cv.use_id(bk72xx_ble.BK72xxBLE),
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
    "bk72xx_ble_tracker.start_scan",
    StartScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BK72xxBLETracker),
            # Optional with no default, unlike esp32_ble_tracker: omitting it
            # keeps whatever scan_parameters.continuous configured, instead of
            # silently forcing one-shot.
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
    "bk72xx_ble_tracker.stop_scan",
    StopScanAction,
    automation.maybe_simple_id(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(BK72xxBLETracker),
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
    cg.add_define("USE_BK72XX_BLE_TRACKER")
    # Compiles the shared adv + scan-response merge (the BDK delivers the pair
    # as separate reports).
    cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_BK72XX_BLE_ID])
    cg.add(var.set_parent(parent))
    # The tracker registers itself as a controller scan listener in setup();
    # request the codegen-sized StaticVector slot for it.
    bk72xx_ble.request_scan_listener_slot()

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(scan[CONF_WINDOW])))
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_configured_continuous(scan[CONF_CONTINUOUS]))
    cg.add(var.set_scan_active(scan[CONF_ACTIVE]))

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
