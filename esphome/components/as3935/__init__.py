import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_PIN

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['sensor', 'binary_sensor']
MULTI_CONF = True

CONF_AS3935_ID = 'as3935_id'

as3935_nds = cg.esphome_ns.namespace('as3935')
AS3935 = as3935_nds.class_('AS3935Component', cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AS3935),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                  pins.validate_has_interrupt),
}).extend(i2c.i2c_device_schema(0x3))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
