import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    ICON_ARROW_EXPAND_VERTICAL,
)

CODEOWNERS = ["@wstrzalka"]
DEPENDENCIES = ["i2c"]

tof10120_ns = cg.esphome_ns.namespace("tof10120")
TOF10120Sensor = tof10120_ns.class_(
    "TOF10120Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UNIT_METER,
        ICON_ARROW_EXPAND_VERTICAL,
        3,
        DEVICE_CLASS_EMPTY,
        STATE_CLASS_MEASUREMENT,
    )
    .extend({cv.GenerateID(): cv.declare_id(TOF10120Sensor)})
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x52))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)
