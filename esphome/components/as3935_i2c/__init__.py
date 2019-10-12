import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import as3935, i2c
from esphome.const import CONF_ID

AUTO_LOAD = ['as3935']
DEPENDENCIES = ['i2c']

as3935_i2c_ns = cg.esphome_ns.namespace('as3935_i2c')
I2CAS3935 = as3935_i2c_ns.class_('I2CAS3935Component', as3935.AS3935, i2c.I2CDevice)

CONFIG_SCHEMA = cv.All(as3935.AS3935_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(I2CAS3935),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x03)))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield as3935.setup_as3935(var, config)
    yield i2c.register_i2c_device(var, config)
