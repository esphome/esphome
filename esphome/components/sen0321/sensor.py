import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
)

CODEOWNERS = ["@notjj"]
DEPENDENCIES = ["i2c"]

sen0321_sensor_ns = cg.esphome_ns.namespace("sen0321_sensor")
Sen0321Sensor = sen0321_sensor_ns.class_(
    "Sen0321Sensor", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Sen0321Sensor,
        unit_of_measurement=UNIT_PARTS_PER_BILLION,
        icon=ICON_CHEMICAL_WEAPON,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x73))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
