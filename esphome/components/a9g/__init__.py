import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import uart
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    UNIT_DEGREES,
    ICON_EMPTY,
    DEVICE_CLASS_EMPTY,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@coogle"]

a9g_ns = cg.esphome_ns.namespace("a9g")
a9g = a9g_ns.class_("A9G", cg.Component, uart.UARTDevice)
# a9gListener = a9g_ns.class_("a9gListener")

CONF_A9G_ID = "a9g_id"
CONF_POWER_ON_PIN = "power_on_pin"
CONF_POWER_OFF_PIN = "power_off_pin"
CONF_WAKE_PIN = "wake_pin"
CONF_LOW_POWER_PIN = "low_power_pin"

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(a9g),
            cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
                UNIT_DEGREES, ICON_EMPTY, 6, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
                UNIT_DEGREES, ICON_EMPTY, 6, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_POWER_ON_PIN, default=16): pins.output_pin,
            cv.Optional(CONF_POWER_OFF_PIN, default=15): pins.output_pin,
            cv.Optional(CONF_WAKE_PIN, default=13): pins.input_pin,
            cv.Optional(CONF_LOW_POWER_PIN, default=2): pins.output_pin,
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_LATITUDE in config:
        sens = await sensor.new_sensor(config[CONF_LATITUDE])
        cg.add(var.set_latitude_sensor(sens))

    if CONF_LONGITUDE in config:
        sens = await sensor.new_sensor(config[CONF_LONGITUDE])
        cg.add(var.set_longitude_sensor(sens))


def validate(config, item_config):
    uart.validate_device("a9g", config, item_config, require_tx=True)
