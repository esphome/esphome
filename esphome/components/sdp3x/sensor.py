import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_PRESSURE,
    ICON_EMPTY,
    UNIT_HECTOPASCAL,
)

DEPENDENCIES = ["i2c"]

sdp3x_ns = cg.esphome_ns.namespace("sdp3x")
SDP3XComponent = sdp3x_ns.class_("SDP3XComponent", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_HECTOPASCAL, ICON_EMPTY, 1, DEVICE_CLASS_PRESSURE)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SDP3XComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x21))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield sensor.register_sensor(var, config)
