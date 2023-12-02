from esphome.components import time
from esphome import automation
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_TRIGGER_ID, CONF_SENSOR_DATAPOINT

DEPENDENCIES = ["uart"]

CONF_IGNORE_MCU_UPDATE_ON_DATAPOINTS = "ignore_mcu_update_on_datapoints"

CONF_ON_DATAPOINT_UPDATE = "on_datapoint_update"
CONF_DATAPOINT_TYPE = "datapoint_type"
CONF_STATUS_PIN = "status_pin"

tuya_ns = cg.esphome_ns.namespace("tuya")
Tuya = tuya_ns.class_("Tuya", cg.Component, uart.UARTDevice)

DPTYPE_ANY = "any"
DPTYPE_RAW = "raw"
DPTYPE_BOOL = "bool"
DPTYPE_INT = "int"
DPTYPE_UINT = "uint"
DPTYPE_STRING = "string"
DPTYPE_ENUM = "enum"
DPTYPE_BITMASK = "bitmask"

DATAPOINT_TYPES = {
    DPTYPE_ANY: tuya_ns.struct("TuyaDatapoint"),
    DPTYPE_RAW: cg.std_vector.template(cg.uint8),
    DPTYPE_BOOL: cg.bool_,
    DPTYPE_INT: cg.int_,
    DPTYPE_UINT: cg.uint32,
    DPTYPE_STRING: cg.std_string,
    DPTYPE_ENUM: cg.uint8,
    DPTYPE_BITMASK: cg.uint32,
}

DATAPOINT_TRIGGERS = {
    DPTYPE_ANY: tuya_ns.class_(
        "TuyaDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_ANY]),
    ),
    DPTYPE_RAW: tuya_ns.class_(
        "TuyaRawDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_RAW]),
    ),
    DPTYPE_BOOL: tuya_ns.class_(
        "TuyaBoolDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_BOOL]),
    ),
    DPTYPE_INT: tuya_ns.class_(
        "TuyaIntDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_INT]),
    ),
    DPTYPE_UINT: tuya_ns.class_(
        "TuyaUIntDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_UINT]),
    ),
    DPTYPE_STRING: tuya_ns.class_(
        "TuyaStringDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_STRING]),
    ),
    DPTYPE_ENUM: tuya_ns.class_(
        "TuyaEnumDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_ENUM]),
    ),
    DPTYPE_BITMASK: tuya_ns.class_(
        "TuyaBitmaskDatapointUpdateTrigger",
        automation.Trigger.template(DATAPOINT_TYPES[DPTYPE_BITMASK]),
    ),
}


def assign_declare_id(value):
    value = value.copy()
    value[CONF_TRIGGER_ID] = cv.declare_id(
        DATAPOINT_TRIGGERS[value[CONF_DATAPOINT_TYPE]]
    )(value[CONF_TRIGGER_ID].id)
    return value


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
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        DATAPOINT_TRIGGERS[DPTYPE_ANY]
                    ),
                    cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
                    cv.Optional(CONF_DATAPOINT_TYPE, default=DPTYPE_ANY): cv.one_of(
                        *DATAPOINT_TRIGGERS, lower=True
                    ),
                },
                extra_validators=assign_declare_id,
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
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], var, conf[CONF_SENSOR_DATAPOINT]
        )
        await automation.build_automation(
            trigger, [(DATAPOINT_TYPES[conf[CONF_DATAPOINT_TYPE]], "x")], conf
        )
