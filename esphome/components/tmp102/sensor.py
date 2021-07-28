"""
The TMP102 is a two-wire, serial output temperature
sensor available in a tiny SOT563 package. Requiring
no external components, the TMP102 is capable of
reading temperatures to a resolution of 0.0625°C.

https://www.sparkfun.com/datasheets/Sensors/Temperature/tmp102.pdf

"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@timsavage"]
DEPENDENCIES = ["i2c"]

tmp102_ns = cg.esphome_ns.namespace("tmp102")
TMP102Component = tmp102_ns.class_(
    "TMP102Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(TMP102Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await sensor.register_sensor(var, config)
