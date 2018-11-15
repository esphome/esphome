import voluptuous as vol

from esphomeyaml import pins
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_PCF8575
from esphomeyaml.helpers import App, GPIOInputPin, GPIOOutputPin, Pvariable, io_ns, setup_component

DEPENDENCIES = ['i2c']

PCF8574GPIOMode = io_ns.enum('PCF8574GPIOMode')
PCF8675_GPIO_MODES = {
    'INPUT': PCF8574GPIOMode.PCF8574_INPUT,
    'INPUT_PULLUP': PCF8574GPIOMode.PCF8574_INPUT_PULLUP,
    'OUTPUT': PCF8574GPIOMode.PCF8574_OUTPUT,
}

PCF8574GPIOInputPin = io_ns.class_('PCF8574GPIOInputPin', GPIOInputPin)
PCF8574GPIOOutputPin = io_ns.class_('PCF8574GPIOOutputPin', GPIOOutputPin)

PCF8574_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(pins.PCF8574Component),
    vol.Optional(CONF_ADDRESS, default=0x21): cv.i2c_address,
    vol.Optional(CONF_PCF8575, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema)

CONFIG_SCHEMA = vol.All(cv.ensure_list, [PCF8574_SCHEMA])


def to_code(config):
    for conf in config:
        rhs = App.make_pcf8574_component(conf[CONF_ADDRESS], conf[CONF_PCF8575])
        var = Pvariable(conf[CONF_ID], rhs)
        setup_component(var, conf)


BUILD_FLAGS = '-DUSE_PCF8574'
