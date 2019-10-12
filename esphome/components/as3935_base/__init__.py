import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_PIN, CONF_INDOOR, CONF_WATCHDOG_THRESHOLD, \
    CONF_NOISE_LEVEL, CONF_SPIKE_REJECTION, CONF_LIGHTNING_THRESHOLD, \
    CONF_MASK_DISTURBER, CONF_DIV_RATIO, CONF_CAP
from esphome.core import coroutine


AUTO_LOAD = ['sensor', 'binary_sensor']
MULTI_CONF = True

CONF_AS3935_ID = 'as3935_id'

as3935_base_ns = cg.esphome_ns.namespace('as3935_base')
AS3935 = as3935_base_ns.class_('AS3935Component', cg.Component)

AS3935_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AS3935),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                  pins.validate_has_interrupt),
    cv.Optional(CONF_INDOOR): cv.boolean,
    cv.Optional(CONF_WATCHDOG_THRESHOLD): cv.int_range(min=1, max=10),
    cv.Optional(CONF_NOISE_LEVEL): cv.int_range(min=1, max=7),
    cv.Optional(CONF_SPIKE_REJECTION): cv.int_range(min=1, max=11),
    cv.Optional(CONF_LIGHTNING_THRESHOLD): cv.one_of(0, 1, 5, 9, 16, int=True),
    cv.Optional(CONF_MASK_DISTURBER): cv.boolean,
    cv.Optional(CONF_DIV_RATIO): cv.one_of(0, 16, 22, 64, 128, int=True),
    cv.Optional(CONF_CAP): cv.int_range(min=0, max=15),
})


@coroutine
def setup_as3935(var, config):
    yield cg.register_component(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    if CONF_INDOOR in config:
        cg.add(var.set_indoor(config[CONF_INDOOR]))
    if CONF_WATCHDOG_THRESHOLD in config:
        cg.add(var.set_watchdog_threshold(config[CONF_WATCHDOG_THRESHOLD]))
    if CONF_NOISE_LEVEL in config:
        cg.add(var.set_noise_level(config[CONF_NOISE_LEVEL]))
    if CONF_SPIKE_REJECTION in config:
        cg.add(var.set_spike_rejection(config[CONF_SPIKE_REJECTION]))
    if CONF_LIGHTNING_THRESHOLD in config:
        cg.add(var.set_lightning_threshold(config[CONF_LIGHTNING_THRESHOLD]))
    if CONF_MASK_DISTURBER in config:
        cg.add(var.set_mask_disturber(config[CONF_MASK_DISTURBER]))
    if CONF_DIV_RATIO in config:
        cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_CAP in config:
        cg.add(var.set_cap(config[CONF_CAP]))
