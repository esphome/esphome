import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_NUMBER, CONF_MODE, CONF_INVERTED

CONF_ON_TIME = 'on_time'
CONF_OFF_TIME = 'off_time'
CONF_RISE_TIME = 'rise_time'
CONF_FALL_TIME = 'fall_time'
CONF_ON_INT = 'on_intensity'
CONF_OFF_INT = 'off_intensity'
CONF_FADING_MODE = 'fading_mode'

DEPENDENCIES = ['i2c']
MULTI_CONF = True

sx1509_ns = cg.esphome_ns.namespace('sx1509')
SX1509GPIOMode = sx1509_ns.enum('SX1509GPIOMode')
SX1509_GPIO_MODES = {
    'INPUT': SX1509GPIOMode.SX1509_INPUT,
    'INPUT_PULLUP': SX1509GPIOMode.SX1509_INPUT_PULLUP,
    'OUTPUT': SX1509GPIOMode.SX1509_OUTPUT,
    'BREATHE_OUTPUT': SX1509GPIOMode.SX1509_BREATHE_OUTPUT,
    'BLINK_OUTPUT': SX1509GPIOMode.SX1509_BLINK_OUTPUT
}
SX1509_FADING_MODES = {
    'LINEAR': SX1509GPIOMode.SX1509_FADING_LINEAR,
    'LOGARITHMIC': SX1509GPIOMode.SX1509_FADING_LOGARITHMIC
}

SX1509Component = sx1509_ns.class_('SX1509Component', cg.Component, i2c.I2CDevice)
SX1509GPIOPin = sx1509_ns.class_('SX1509GPIOPin', cg.GPIOPin)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SX1509Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x3E))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)


CONF_SX1509 = 'sx1509'
SX1509_OUTPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_SX1509): cv.use_id(SX1509Component),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="OUTPUT"): cv.enum(SX1509_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    cv.Optional(CONF_FADING_MODE, default="LINEAR"): cv.enum(SX1509_FADING_MODES, upper=True),
    cv.Optional(CONF_ON_TIME, default=0): cv.int_,
    cv.Optional(CONF_OFF_TIME, default=0): cv.int_,
    cv.Optional(CONF_RISE_TIME, default=0): cv.int_,
    cv.Optional(CONF_FALL_TIME, default=0): cv.int_,
    cv.Optional(CONF_ON_INT, default=255): cv.int_range(min=0, max=255),
    cv.Optional(CONF_OFF_INT, default=0): cv.int_range(min=0, max=255), })
SX1509_INPUT_PIN_SCHEMA = cv.Schema({
    cv.Required(CONF_SX1509): cv.use_id(SX1509Component),
    cv.Required(CONF_NUMBER): cv.int_,
    cv.Optional(CONF_MODE, default="INPUT"): cv.enum(SX1509_GPIO_MODES, upper=True),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean,
})


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SX1509,
                                   (SX1509_OUTPUT_PIN_SCHEMA, SX1509_INPUT_PIN_SCHEMA))
def sx1509_pin_to_code(config):
    parent = yield cg.get_variable(config[CONF_SX1509])
    if(config[CONF_MODE] == 'BREATHE_OUTPUT' or config[CONF_MODE] == 'BLINK_OUTPUT'):
        yield SX1509GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED],
                                config[CONF_ON_TIME], config[CONF_OFF_TIME], config[CONF_ON_INT], config[CONF_OFF_INT],
                                config[CONF_RISE_TIME], config[CONF_FALL_TIME], config[CONF_FADING_MODE])
    else:
        yield SX1509GPIOPin.new(parent, config[CONF_NUMBER], config[CONF_MODE], config[CONF_INVERTED])
