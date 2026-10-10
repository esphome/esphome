from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT, CONF_TIME_ID
from esphome.cpp_generator import MockObj

DEPENDENCIES = ["uart"]
DOMAIN = "tuya"

CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"

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
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Tuya),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS): cv.ensure_list(
                cv.uint8_t
            ),
            cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
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
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_id(time_))
    if CONF_STATUS_PIN in config:
        status_pin_ = await cg.gpio_pin_expression(config[CONF_STATUS_PIN])
        cg.add(var.set_status_pin(status_pin_))
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
