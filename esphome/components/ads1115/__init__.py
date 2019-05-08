import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['sensor', 'voltage_sampler']
MULTI_CONF = True

ads1115_ns = cg.esphome_ns.namespace('ads1115')
ADS1115Component = ads1115_ns.class_('ADS1115Component', cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ADS1115Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(None))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
