import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c"]

ain4_20ma_ns = cg.esphome_ns.namespace("ain4_20ma")

Ain420maComponent = ain4_20ma_ns.class_(
    "Ain420maComponent", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Ain420maComponent,
        unit_of_measurement="mA",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(default_address=0x55))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
