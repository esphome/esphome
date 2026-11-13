from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_HEARTBEAT,
    CONF_ID,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_SENSOR_DATAPOINT,
    CONF_TIME_ID,
    CONF_TX_PIN,
    CONF_UART_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
)
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]
DOMAIN = "tuya"

CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"
CONF_FORCE_TIME_SYNC = "force_time_sync"
CONF_MCU_WATCHDOG = "mcu_watchdog"
CONF_RESET_TIME = "reset_time"
CONF_MISSED_HEARTBEATS = "missed_heartbeats"
CONF_DATAPOINT_ACK = "datapoint_ack"

ACTION_REBOOT = "reboot"
ACTION_RESET = "reset"
ACTION_DISABLE = "disable"
RESET_PLATFORMS = [
    PLATFORM_BK72XX,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
    PLATFORM_ESP8266,
    PLATFORM_ESP32,
]

tuya_ns = cg.esphome_ns.namespace("tuya")
TuyaDatapointType = tuya_ns.enum("TuyaDatapointType", is_class=True)
Tuya = tuya_ns.class_("Tuya", cg.Component, uart.UARTDevice)

TuyaDatapoint = tuya_ns.struct("TuyaDatapoint")

DPTYPE_ANY = "any"
DPTYPE_RAW = "raw"
DPTYPE_BOOL = "bool"
DPTYPE_INT = "int"
DPTYPE_UINT = "uint"
DPTYPE_STRING = "string"
DPTYPE_ENUM = "enum"
DPTYPE_BITMASK = "bitmask"

# Automation argument type, plus the expected TuyaDatapointType and the field forwarded
DATAPOINT_TYPES = {
    DPTYPE_ANY: (TuyaDatapoint, None, None),
    DPTYPE_RAW: (cg.std_vector.template(cg.uint8), TuyaDatapointType.RAW, "value_raw"),
    DPTYPE_BOOL: (cg.bool_, TuyaDatapointType.BOOLEAN, "value_bool"),
    DPTYPE_INT: (cg.int_, TuyaDatapointType.INTEGER, "value_int"),
    DPTYPE_UINT: (cg.uint32, TuyaDatapointType.INTEGER, "value_uint"),
    DPTYPE_STRING: (cg.std_string, TuyaDatapointType.STRING, "value_string"),
    DPTYPE_ENUM: (cg.uint8, TuyaDatapointType.ENUM, "value_enum"),
    DPTYPE_BITMASK: (cg.uint32, TuyaDatapointType.BITMASK, "value_bitmask"),
}


CONF_TUYA_ID = "tuya_id"


def _validate_mcu_watchdog(config: ConfigType) -> ConfigType:
    if config[CONF_ACTION] != ACTION_RESET:
        return config
    cv.only_on(RESET_PLATFORMS)(config)
    if CONF_TX_PIN not in config:
        raise cv.Invalid(
            f"{CONF_ACTION}: {ACTION_RESET} requires {CONF_TX_PIN}", path=[CONF_ACTION]
        )
    if config[CONF_TX_PIN][CONF_INVERTED]:
        raise cv.Invalid(
            f"{CONF_TX_PIN} must not be inverted", path=[CONF_TX_PIN, CONF_INVERTED]
        )
    return config


def _validate_time_options(config: ConfigType) -> ConfigType:
    if config[CONF_FORCE_TIME_SYNC] and CONF_TIME_ID not in config:
        raise cv.Invalid(
            f"{CONF_FORCE_TIME_SYNC} requires {CONF_TIME_ID}",
            path=[CONF_FORCE_TIME_SYNC],
        )
    return config


def _validate_heartbeat(config: ConfigType) -> ConfigType:
    # runs before the defaults: only a watchdog set in the config is rejected
    if cv.boolean(config.get(CONF_HEARTBEAT, True)):
        return config
    watchdog = config.get(CONF_MCU_WATCHDOG) or {}
    if CONF_MCU_WATCHDOG in config and watchdog.get(CONF_ACTION) != ACTION_DISABLE:
        raise cv.Invalid(
            f"{CONF_MCU_WATCHDOG} requires {CONF_HEARTBEAT}",
            path=[CONF_MCU_WATCHDOG],
        )
    return config


CONFIG_SCHEMA = cv.All(
    _validate_heartbeat,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Tuya),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_FORCE_TIME_SYNC, default=False): cv.boolean,
            cv.Optional(CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS): cv.ensure_list(
                cv.uint8_t
            ),
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_HEARTBEAT, default=True): cv.boolean,
            cv.Optional(CONF_DATAPOINT_ACK, default=False): cv.boolean,
            cv.Optional(CONF_MCU_WATCHDOG, default={}): cv.All(
                cv.Schema(
                    {
                        cv.Optional(CONF_ACTION, default=ACTION_REBOOT): cv.one_of(
                            ACTION_REBOOT, ACTION_RESET, ACTION_DISABLE, lower=True
                        ),
                        cv.Optional(CONF_MISSED_HEARTBEATS, default=6): cv.int_range(
                            min=1, max=20
                        ),
                        cv.Optional(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
                        cv.Optional(CONF_RESET_TIME, default="500ms"): cv.All(
                            cv.positive_time_period_milliseconds,
                            cv.Range(
                                min=cv.TimePeriod(milliseconds=50),
                                max=cv.TimePeriod(seconds=5),
                            ),
                        ),
                    }
                ),
                _validate_mcu_watchdog,
            ),
            cv.Optional(CONF_ON_DATAPOINT_UPDATE): automation.validate_automation(
                {
                    cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
                    cv.Optional(CONF_DATAPOINT_TYPE, default=DPTYPE_ANY): cv.one_of(
                        *DATAPOINT_TYPES, lower=True
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_time_options,
)


def _final_validate(config: ConfigType) -> ConfigType:
    watchdog = config[CONF_MCU_WATCHDOG]
    if watchdog[CONF_ACTION] != ACTION_RESET:
        return config
    path = [CONF_MCU_WATCHDOG, CONF_TX_PIN]
    bus = fv.get_declaring_config(config[CONF_UART_ID])
    if str(bus[CONF_ID].type) not in uart.NATIVE_UART_CLASSES:
        raise cv.Invalid(
            f"{CONF_ACTION}: {ACTION_RESET} requires a UART bus of the microcontroller itself",
            path=[CONF_MCU_WATCHDOG, CONF_ACTION],
        )
    if (
        CONF_TX_PIN not in bus
        or bus[CONF_TX_PIN][CONF_NUMBER] != watchdog[CONF_TX_PIN][CONF_NUMBER]
    ):
        raise cv.Invalid(
            f"{CONF_TX_PIN} must be the {CONF_TX_PIN} of the UART bus used by tuya",
            path=path,
        )
    if bus[CONF_TX_PIN][CONF_INVERTED]:
        raise cv.Invalid(
            f"{CONF_ACTION}: {ACTION_RESET} requires a UART bus with a {CONF_TX_PIN} that is not inverted",
            path=[CONF_MCU_WATCHDOG, CONF_ACTION],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await uart.new_uart_component(config)
    if (time_id := config.get(CONF_TIME_ID)) is not None:
        await cg.add_variable(var.set_time_id, time_id)
        if config[CONF_FORCE_TIME_SYNC]:
            cg.add(var.set_force_time_sync(True))
    await cg.add_pin(var.set_status_pin, config.get(CONF_STATUS_PIN))
    if not config[CONF_HEARTBEAT]:
        cg.add(var.set_heartbeat(False))
    if config[CONF_DATAPOINT_ACK]:
        cg.add(var.set_datapoint_ack(True))
    watchdog = config[CONF_MCU_WATCHDOG]
    if config[CONF_HEARTBEAT] and watchdog[CONF_ACTION] != ACTION_DISABLE:
        cg.add_define("USE_TUYA_MCU_WATCHDOG")
        cg.add(var.set_mcu_watchdog(watchdog[CONF_MISSED_HEARTBEATS]))
        if watchdog[CONF_ACTION] == ACTION_RESET:
            cg.add_define("USE_TUYA_MCU_WATCHDOG_RESET")
            tx_pin = await cg.gpio_pin_expression(watchdog[CONF_TX_PIN])
            cg.add(
                var.set_mcu_watchdog_reset(
                    tx_pin, watchdog[CONF_RESET_TIME].total_milliseconds
                )
            )
    if CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS in config:
        for dp in config[CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS]:
            cg.add(var.add_ignore_mcu_update_on_datapoints(dp))
    for conf in config.get(CONF_ON_DATAPOINT_UPDATE, []):
        type_, expected, field = DATAPOINT_TYPES[conf[CONF_DATAPOINT_TYPE]]
        forward = None
        if expected is not None:
            forward = [getattr(MockObj("x", ".").expect_type(expected), field)]
        callback = await automation.build_trigger_callback(
            [(type_, "x")], conf, params=[(TuyaDatapoint, "x")], forward=forward
        )
        cg.add(var.register_listener(conf[CONF_SENSOR_DATAPOINT], callback))
