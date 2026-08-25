"""Realtek BLE scanner (RTL8720C/D).

Drives the Realtek GAP scan stack (via the rtl87xx_ble controller) and
implements the platform-neutral ble_device_base BLEHub contract, so the BLE
advertisement sensors and bluetooth_proxy bind to it through cv.use_id(BLEHub)
with no RTL-specific code.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base, ota, rtl87xx_ble
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
from esphome.core import ID, TimePeriod
from esphome.types import ConfigType

DEPENDENCIES = ["rtl87xx"]
AUTO_LOAD = ["ble_device_base", "rtl87xx_ble"]

# The consumer owns the id constant (bk72xx/ln882h pattern).
CONF_RTL87XX_BLE_ID = "rtl87xx_ble_id"
CODEOWNERS = ["@Bl00d-B0b"]

ble_device_base.register_hub_provider("rtl87xx_ble_tracker")


def _ceil_ms(value: TimePeriod) -> int:
    # The controller takes whole milliseconds; round up so the shared validator's
    # 2.5 ms floor cannot truncate below the controller minimum.
    return -(-value.total_microseconds // 1000)


rtl87xx_ble_tracker_ns = cg.esphome_ns.namespace("rtl87xx_ble_tracker")
RTL87xxBLETracker = rtl87xx_ble_tracker_ns.class_(
    "RTL87xxBLETracker", ble_device_base.BLEHub, cg.Component
)

StartScanAction = rtl87xx_ble_tracker_ns.class_("StartScanAction", automation.Action)
StopScanAction = rtl87xx_ble_tracker_ns.class_("StopScanAction", automation.Action)

ESPBTAdvertiseTrigger = ble_automation.ESPBTAdvertiseTrigger
BLEServiceDataAdvertiseTrigger = ble_automation.BLEServiceDataAdvertiseTrigger
BLEManufacturerDataAdvertiseTrigger = ble_automation.BLEManufacturerDataAdvertiseTrigger
BLEEndOfScanTrigger = ble_automation.BLEEndOfScanTrigger

# The Realtek GAP reference scan rate is slow (820 ms); use the same 100/30 ms
# 30 % duty cycle the other single-radio LibreTiny trackers use.
SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema("100ms")


def _validate_rounded_pair(config: ConfigType) -> ConfigType:
    # "What is validated is what is programmed": the shared validator checks the
    # raw values against the 0.625 ms grid, but this controller takes whole
    # milliseconds and both values round UP independently. Same rule as
    # validate_scan_parameters, applied to the rounded values: an explicit
    # window == interval stays allowed, a silent collapse to it does not.
    scan = config[CONF_SCAN_PARAMETERS]
    raw_interval = scan[CONF_INTERVAL]
    raw_window = scan[CONF_WINDOW]
    interval = _ceil_ms(raw_interval)
    window = _ceil_ms(raw_window)
    # Rounding cannot invert the pair (the shared validator already rejects
    # window > interval, and ceil is monotonic), so only the collapse is left.
    if window == interval and raw_window < raw_interval:
        raise cv.Invalid(
            f"Scan window ({raw_window}) and interval ({raw_interval}) both "
            f"round up to {interval} ms, which the controller scans at a 100 % "
            f"duty cycle. Separate them by at least 1 ms."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RTL87xxBLETracker),
            cv.GenerateID(CONF_RTL87XX_BLE_ID): cv.use_id(rtl87xx_ble.RTL87xxBLE),
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
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_rounded_pair,
)


async def to_code(config: ConfigType) -> None:
    # Selects the BLEHub alias arm in ble_device_base/ble_hub_impl.h.
    cg.add_define("USE_RTL87XX_BLE_TRACKER")
    # Realtek GAP delivers advertisement and scan response as separate reports.
    cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_RTL87XX_BLE_ID])
    cg.add(var.set_parent(parent))

    # Get notified when an OTA update starts, to pause scanning (esp32_ble_tracker parity)
    ota.request_ota_state_listeners()

    scan = config[CONF_SCAN_PARAMETERS]
    # The controller takes milliseconds and converts to the controller's 0.625 ms units;
    # sub-millisecond values round UP so the validated 2.5 ms floor holds.
    cg.add(var.set_scan_duration(scan[CONF_DURATION].total_milliseconds))
    cg.add(var.set_scan_interval(_ceil_ms(scan[CONF_INTERVAL])))
    cg.add(var.set_scan_window(_ceil_ms(scan[CONF_WINDOW])))
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


@automation.register_action(
    "rtl87xx_ble_tracker.start_scan",
    StartScanAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RTL87xxBLETracker),
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
        cg.add(var.set_continuous(await cg.templatable(continuous, args, cg.bool_)))
    return var


@automation.register_action(
    "rtl87xx_ble_tracker.stop_scan",
    StopScanAction,
    automation.maybe_simple_id(
        cv.Schema({cv.GenerateID(): cv.use_id(RTL87xxBLETracker)})
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
