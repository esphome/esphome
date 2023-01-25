import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ACTIVE,
    CONF_ID,
    CONF_INTERVAL,
    CONF_DURATION,
    CONF_TRIGGER_ID,
    CONF_MAC_ADDRESS,
    CONF_SERVICE_UUID,
    CONF_MANUFACTURER_ID,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
)
from esphome.components import esp32_ble
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option

AUTO_LOAD = ["esp32_ble"]
DEPENDENCIES = ["esp32"]

CONF_ESP32_BLE_ID = "esp32_ble_id"
CONF_SCAN_PARAMETERS = "scan_parameters"
CONF_WINDOW = "window"
CONF_CONTINUOUS = "continuous"
CONF_ON_SCAN_END = "on_scan_end"
esp32_ble_tracker_ns = cg.esphome_ns.namespace("esp32_ble_tracker")
ESP32BLETracker = esp32_ble_tracker_ns.class_(
    "ESP32BLETracker",
    cg.Component,
    esp32_ble.GAPEventHandler,
    esp32_ble.GATTcEventHandler,
    cg.Parented.template(esp32_ble.ESP32BLE),
)
ESPBTClient = esp32_ble_tracker_ns.class_("ESPBTClient")
ESPBTDeviceListener = esp32_ble_tracker_ns.class_("ESPBTDeviceListener")
ESPBTDevice = esp32_ble_tracker_ns.class_("ESPBTDevice")
ESPBTDeviceConstRef = ESPBTDevice.operator("ref").operator("const")
adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")
# Triggers
ESPBTAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
BLEServiceDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEServiceDataAdvertiseTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEManufacturerDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEManufacturerDataAdvertiseTrigger",
    automation.Trigger.template(adv_data_t_const_ref),
)
BLEEndOfScanTrigger = esp32_ble_tracker_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)
# Actions
ESP32BLEStartScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStartScanAction", automation.Action
)
ESP32BLEStopScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStopScanAction", automation.Action
)


def validate_scan_parameters(config):
    duration = config[CONF_DURATION]
    interval = config[CONF_INTERVAL]
    window = config[CONF_WINDOW]

    if window > interval:
        raise cv.Invalid(
            f"Scan window ({window}) needs to be smaller than scan interval ({interval})"
        )

    if interval.total_milliseconds * 3 > duration.total_milliseconds:
        raise cv.Invalid(
            "Scan duration needs to be at least three times the scan interval to"
            "cover all BLE channels."
        )

    return config


bt_uuid16_format = "XXXX"
bt_uuid32_format = "XXXXXXXX"
bt_uuid128_format = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"


def bt_uuid(value):
    in_value = cv.string_strict(value)
    value = in_value.upper()

    if len(value) == len(bt_uuid16_format):
        pattern = re.compile("^[A-F|0-9]{4,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 16 bit UUID format: '{in_value}'"
            )
        return value
    if len(value) == len(bt_uuid32_format):
        pattern = re.compile("^[A-F|0-9]{8,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 32 bit UUID format: '{in_value}'"
            )
        return value
    if len(value) == len(bt_uuid128_format):
        pattern = re.compile(
            "^[A-F|0-9]{8,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{12,}$"
        )
        if not pattern.match(value):
            raise cv.Invalid(
                f"Invalid hexadecimal value for 128 UUID format: '{in_value}'"
            )
        return value
    raise cv.Invalid(
        f"Service UUID must be in 16 bit '{bt_uuid16_format}', 32 bit '{bt_uuid32_format}', or 128 bit '{bt_uuid128_format}' format"
    )


def as_hex(value):
    return cg.RawExpression(f"0x{value}ULL")


def as_hex_array(value):
    value = value.replace("-", "")
    cpp_array = [
        f"0x{part}" for part in [value[i : i + 2] for i in range(0, len(value), 2)]
    ]
    return cg.RawExpression(f"(uint8_t*)(const uint8_t[16]){{{','.join(cpp_array)}}}")


def as_reversed_hex_array(value):
    value = value.replace("-", "")
    cpp_array = [
        f"0x{part}" for part in [value[i : i + 2] for i in range(0, len(value), 2)]
    ]
    return cg.RawExpression(
        f"(uint8_t*)(const uint8_t[16]){{{','.join(reversed(cpp_array))}}}"
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLETracker),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): cv.All(
            cv.Schema(
                {
                    cv.Optional(
                        CONF_DURATION, default="5min"
                    ): cv.positive_time_period_seconds,
                    cv.Optional(
                        CONF_INTERVAL, default="320ms"
                    ): cv.positive_time_period_milliseconds,
                    cv.Optional(
                        CONF_WINDOW, default="30ms"
                    ): cv.positive_time_period_milliseconds,
                    cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
                    cv.Optional(CONF_CONTINUOUS, default=True): cv.boolean,
                }
            ),
            validate_scan_parameters,
        ),
        cv.Optional(CONF_ON_BLE_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPBTAdvertiseTrigger),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            }
        ),
        cv.Optional(CONF_ON_BLE_SERVICE_DATA_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEServiceDataAdvertiseTrigger
                ),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Required(CONF_SERVICE_UUID): bt_uuid,
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
                cv.Required(CONF_MANUFACTURER_ID): bt_uuid,
            }
        ),
        cv.Optional(CONF_ON_SCAN_END): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEEndOfScanTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant

ESP_BLE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_id(ESP32BLETracker),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(parent.register_gap_event_handler(var))
    cg.add(parent.register_gattc_event_handler(var))
    cg.add(var.set_parent(parent))

    params = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_duration(params[CONF_DURATION]))
    cg.add(var.set_scan_interval(int(params[CONF_INTERVAL].total_milliseconds / 0.625)))
    cg.add(var.set_scan_window(int(params[CONF_WINDOW].total_milliseconds / 0.625)))
    cg.add(var.set_scan_active(params[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(params[CONF_CONTINUOUS]))
    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
    for conf in config.get(CONF_ON_BLE_SERVICE_DATA_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_SERVICE_UUID]) == len(bt_uuid16_format):
            cg.add(trigger.set_service_uuid16(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid32_format):
            cg.add(trigger.set_service_uuid32(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid128_format):
            uuid128 = as_reversed_hex_array(conf[CONF_SERVICE_UUID])
            cg.add(trigger.set_service_uuid128(uuid128))
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    for conf in config.get(CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid16_format):
            cg.add(trigger.set_manufacturer_uuid16(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid32_format):
            cg.add(trigger.set_manufacturer_uuid32(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid128_format):
            uuid128 = as_reversed_hex_array(conf[CONF_MANUFACTURER_ID])
            cg.add(trigger.set_manufacturer_uuid128(uuid128))
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
    for conf in config.get(CONF_ON_SCAN_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
        # https://github.com/espressif/esp-idf/issues/4101
        # https://github.com/espressif/esp-idf/issues/2503
        # Match arduino CONFIG_BTU_TASK_STACK_SIZE
        # https://github.com/espressif/arduino-esp32/blob/fd72cf46ad6fc1a6de99c1d83ba8eba17d80a4ee/tools/sdk/esp32/sdkconfig#L1866
        add_idf_sdkconfig_option("CONFIG_BTU_TASK_STACK_SIZE", 8192)

    cg.add_define("USE_OTA_STATE_CALLBACK")  # To be notified when an OTA update starts
    cg.add_define("USE_ESP32_BLE_CLIENT")


ESP32_BLE_START_SCAN_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ESP32BLETracker),
        cv.Optional(CONF_CONTINUOUS, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "esp32_ble_tracker.start_scan",
    ESP32BLEStartScanAction,
    ESP32_BLE_START_SCAN_ACTION_SCHEMA,
)
async def esp32_ble_tracker_start_scan_action_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_continuous(config[CONF_CONTINUOUS]))
    return var


ESP32_BLE_STOP_SCAN_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ESP32BLETracker),
        }
    )
)


@automation.register_action(
    "esp32_ble_tracker.stop_scan",
    ESP32BLEStopScanAction,
    ESP32_BLE_STOP_SCAN_ACTION_SCHEMA,
)
async def esp32_ble_tracker_stop_scan_action_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def register_ble_device(var, config):
    paren = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_listener(var))
    return var


async def register_client(var, config):
    paren = await cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_client(var))
    return var
