import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_FREQUENCY, CONF_ID

DEPENDENCIES = ['i2c']
MULTI_CONF = True

sx1509_ns = cg.esphome_ns.namespace('sx1509')
SX1509Component = sx1509_ns.class_('SX1509Component', cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SX1509Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x3E))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
