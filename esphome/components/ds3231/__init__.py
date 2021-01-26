import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import (CONF_ID)

CODEOWNERS = ['@snakeye']
DEPENDENCIES = ['i2c']

ds3231_ns = cg.esphome_ns.namespace('ds3231')
DS3231Component = ds3231_ns.class_('DS3231Component', cg.PollingComponent, i2c.I2CDevice)

CONF_DS3231_ID = 'ds3231_id'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DS3231Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x68))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
