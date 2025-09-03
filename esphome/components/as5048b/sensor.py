import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_WIND_DIRECTION,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
)

CODEOWNERS = ["@jhandke"]
DEPENDENCIES = ["i2c"]

as5048b_ns = cg.esphome_ns.namespace("as5048b")
AS5048bComponent = as5048b_ns.class_("AS5048bComponent", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        AS5048bComponent,
        unit_of_measurement=UNIT_DEGREES,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_WIND_DIRECTION,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x43))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
