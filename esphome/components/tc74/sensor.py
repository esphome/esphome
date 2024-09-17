"""
The TC74 is an i2c temperature sensor available in a breadboard-friendly 5-pin
TO-220 package and a SOT-23 SMD package. It has a temperature tolerance of ±2°C
from 25°C-85°C.

https://www.adafruit.com/product/4375

"""

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@sethgirvan"]
DEPENDENCIES = ["i2c"]

tc74_ns = cg.esphome_ns.namespace("tc74")
TC74Component = tc74_ns.class_("TC74Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        TC74Component,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
