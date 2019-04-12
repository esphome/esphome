import voluptuous as vol

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ADDRESS, CONF_FREQUENCY, CONF_ID, CONF_I2C_ID

DEPENDENCIES = ['i2c']
MULTI_CONF = True

pca9685_ns = cg.esphome_ns.namespace('pca9685')
PCA9685Output = pca9685_ns.class_('PCA9685Output', cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(PCA9685Output),
    cv.GenerateID(CONF_I2C_ID): cv.use_variable_id(i2c.I2CComponent),
    vol.Required(CONF_FREQUENCY): vol.All(cv.frequency,
                                          vol.Range(min=23.84, max=1525.88)),
    vol.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_I2C_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren, config[CONF_ADDRESS], config[CONF_FREQUENCY])
    yield cg.register_component(var, config)
