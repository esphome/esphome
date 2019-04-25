import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']

mpr121_ns = cg.esphome_ns.namespace('mpr121')
CONF_MPR121_ID = 'mpr121_id'
MPR121Component = mpr121_ns.class_('MPR121Component', cg.Component, i2c.I2CDevice)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MPR121Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x5A))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
