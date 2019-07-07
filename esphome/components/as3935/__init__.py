import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_PIN, CONF_INDOOR, CONF_WATCHDOG_THRESHOLD, \
    CONF_NOISE_LEVEL, CONF_SPIKE_REJECTION, CONF_LIGHTNING_THRESHOLD, \
    CONF_MASK_DISTURBER, CONF_DIV_RATIO, CONF_CAP

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
    cv.Optional(CONF_INDOOR, default=True): cv.boolean,
    cv.Optional(CONF_WATCHDOG_THRESHOLD, default=2): cv.int_range(min=1, max=10),
    cv.Optional(CONF_NOISE_LEVEL, default=2): cv.int_range(min=1, max=7),
    cv.Optional(CONF_SPIKE_REJECTION, default=1): cv.int_range(min=1, max=11),
    cv.Optional(CONF_LIGHTNING_THRESHOLD, default=0): cv.one_of(0, 1, 5, 9, 16),
    cv.Optional(CONF_MASK_DISTURBER, default=True): cv.boolean,
    cv.Optional(CONF_DIV_RATIO, default=0): cv.one_of(0, 16, 22, 64, 128),
    cv.Optional(CONF_CAP, default=0): cv.int_range(min=0, max=15),
}).extend(i2c.i2c_device_schema(0x3))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_indoor(config[CONF_INDOOR]))
    cg.add(var.set_watchdog_threshold(config[CONF_WATCHDOG_THRESHOLD]))
    cg.add(var.set_noise_level(config[CONF_NOISE_LEVEL]))
    cg.add(var.set_spike_rejection(config[CONF_SPIKE_REJECTION]))
    cg.add(var.set_lightning_threshold(config[CONF_LIGHTNING_THRESHOLD]))
    cg.add(var.set_mask_disturber(config[CONF_MASK_DISTURBER]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    cg.add(var.set_cap(config[CONF_CAP]))
