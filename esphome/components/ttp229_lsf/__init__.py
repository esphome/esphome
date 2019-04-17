import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']

CONF_TTP229_ID = 'ttp229_id'
ttp229_ns = cg.esphome_ns.namespace('ttp229')

TTP229LSFComponent = ttp229_ns.class_('TTP229LSFComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(TTP229LSFComponent),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x57))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
