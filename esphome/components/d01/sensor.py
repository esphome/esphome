import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_2_5,
    DEVICE_CLASS_PM25,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

from . import D01Component

DEPENDENCIES = ["uart"]

CONFIG_SCHEMA = (cv.Schema({
    cv.GenerateID():
    cv.declare_id(D01Component),
    cv.Required(CONF_PM_2_5):
    sensor.sensor_schema(
        unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_PM25,
    ),
}).extend(cv.polling_component_schema("10s")).extend(uart.UART_DEVICE_SCHEMA))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    sens = await sensor.new_sensor(config[CONF_PM_2_5])
    cg.add(var.set_pm25_sensor(sens))
