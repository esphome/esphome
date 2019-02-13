import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_PCF8575
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, GPIOInputPin, GPIOOutputPin, io_ns

DEPENDENCIES = ['i2c']
MULTI_CONF = True

PCF8574GPIOMode = io_ns.enum('PCF8574GPIOMode')
PCF8675_GPIO_MODES = {
    'INPUT': PCF8574GPIOMode.PCF8574_INPUT,
    'INPUT_PULLUP': PCF8574GPIOMode.PCF8574_INPUT_PULLUP,
    'OUTPUT': PCF8574GPIOMode.PCF8574_OUTPUT,
}

PCF8574GPIOInputPin = io_ns.class_('PCF8574GPIOInputPin', GPIOInputPin)
PCF8574GPIOOutputPin = io_ns.class_('PCF8574GPIOOutputPin', GPIOOutputPin)

CONFIG_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(pins.PCF8574Component),
    vol.Optional(CONF_ADDRESS, default=0x21): cv.i2c_address,
    vol.Optional(CONF_PCF8575, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_pcf8574_component(config[CONF_ADDRESS], config[CONF_PCF8575])
    var = Pvariable(config[CONF_ID], rhs)
    setup_component(var, config)


BUILD_FLAGS = '-DUSE_PCF8574'
