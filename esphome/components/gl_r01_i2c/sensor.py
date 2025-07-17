import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
)

CODEOWNERS = ["@pkejval"]
DEPENDENCIES = ["i2c"]

gl_r01_i2c_ns = cg.esphome_ns.namespace("gl_r01_i2c")
GLR01I2CComponent = gl_r01_i2c_ns.class_(
    "GLR01I2CComponent", i2c.I2CDevice, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        GLR01I2CComponent,
        unit_of_measurement=UNIT_MILLIMETER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x74))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)
