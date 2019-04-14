import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_I2C_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']

mpr121_ns = cg.esphome_ns.namespace('mpr121')
CONF_MPR121_ID = 'mpr121_id'
MPR121Component = mpr121_ns.class_('MPR121Component', cg.Component, i2c.I2CDevice)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(MPR121Component),
    cv.GenerateID(CONF_I2C_ID): cv.use_variable_id(i2c.I2CComponent),
    cv.Optional(CONF_ADDRESS, default=0x5A): cv.i2c_address
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    i2c_ = yield cg.get_variable(CONF_I2C_ID)
    var = cg.new_Pvariable(config[CONF_ID], i2c_, config[CONF_ADDRESS])
    yield cg.register_component(var)
