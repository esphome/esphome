import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_SCAN

DEPENDENCIES = ['i2c']

tca9548a_ns = cg.esphome_ns.namespace('tca9548a')
TCA9548AComponent = tca9548a_ns.class_('TCA9548AComponent', cg.PollingComponent, i2c.I2CDevice)

MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TCA9548AComponent),
    cv.Optional(CONF_SCAN, default=True): cv.boolean,
}).extend(i2c.i2c_device_schema(0x70))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    cg.add(var.set_scan(config[CONF_SCAN]))
