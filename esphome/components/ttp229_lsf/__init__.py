import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_ADDRESS, CONF_I2C_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']

CONF_TTP229_ID = 'ttp229_id'
ttp229_ns = cg.esphome_ns.namespace('ttp229')

TTP229LSFComponent = ttp229_ns.class_('TTP229LSFComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(TTP229LSFComponent),
    cv.GenerateID(CONF_I2C_ID): cv.use_variable_id(i2c.I2CComponent),
    cv.Optional(CONF_ADDRESS, default=0x57): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    i2c_ = yield cg.get_variable(config[CONF_I2C_ID])
    var = cg.new_Pvariable(config[CONF_ID], i2c_, config[CONF_ADDRESS])
    yield cg.register_component(var, config)
