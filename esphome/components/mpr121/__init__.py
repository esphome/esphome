import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_RELEASE_DEBOUNCE, CONF_TOUCH_DEBOUNCE

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['binary_sensor']

mpr121_ns = cg.esphome_ns.namespace('mpr121')
CONF_MPR121_ID = 'mpr121_id'
MPR121Component = mpr121_ns.class_('MPR121Component', cg.Component, i2c.I2CDevice)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MPR121Component),
    cv.Optional(CONF_RELEASE_DEBOUNCE):  cv.All(cv.Coerce(int), cv.Range(min=0, max=7)),
    cv.Optional(CONF_TOUCH_DEBOUNCE):  cv.All(cv.Coerce(int), cv.Range(min=0, max=7))
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x5A))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_TOUCH_DEBOUNCE in config:
        cg.add(var.set_touch_debounce(config[CONF_TOUCH_DEBOUNCE]))
    if CONF_RELEASE_DEBOUNCE in config:
        cg.add(var.set_release_debounce(config[CONF_RELEASE_DEBOUNCE]))
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
