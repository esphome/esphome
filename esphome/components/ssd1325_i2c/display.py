import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ssd1325_base, i2c
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

AUTO_LOAD = ['ssd1325_base']
DEPENDENCIES = ['i2c']

ssd1325_i2c = cg.esphome_ns.namespace('ssd1325_i2c')
I2CSSD1325 = ssd1325_i2c.class_('I2CSSD1325', ssd1325_base.SSD1325, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(ssd1325_base.SSD1325_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(I2CSSD1325),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x3C)),
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield ssd1325_base.setup_ssd1036(var, config)
    yield i2c.register_i2c_device(var, config)
