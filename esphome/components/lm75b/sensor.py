import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@beormund"]
DEPENDENCIES = ["i2c"]

lm75b_ns = cg.esphome_ns.namespace("lm75b")
LM75BComponent = lm75b_ns.class_(
    "LM75BComponent", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        LM75BComponent,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=3,
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
