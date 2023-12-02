import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_DISTANCE,
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
)

CODEOWNERS = ["@tathamoddie"]
DEPENDENCIES = ["uart"]

ajsr04m_ns = cg.esphome_ns.namespace("ajsr04m")
AJSR04MComponent = ajsr04m_ns.class_("AJSR04MComponent", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AJSR04MComponent),
        cv.Required(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CENTIMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ajsr04m", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(var.set_distance_sensor(sens))
