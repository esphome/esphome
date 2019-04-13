from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_PCF8575


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

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_variable_id(pins.PCF8574Component),
    cv.Optional(CONF_ADDRESS, default=0x21): cv.i2c_address,
    cv.Optional(CONF_PCF8575, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_pcf8574_component(config[CONF_ADDRESS], config[CONF_PCF8575])
    var = Pvariable(config[CONF_ID], rhs)
    register_component(var, config)


BUILD_FLAGS = '-DUSE_PCF8574'
