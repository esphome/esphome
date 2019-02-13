import voluptuous as vol

from esphome.components import i2c, output
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_FREQUENCY, CONF_ID
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['i2c']
MULTI_CONF = True

PCA9685OutputComponent = output.output_ns.class_('PCA9685OutputComponent',
                                                 Component, i2c.I2CDevice)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(PCA9685OutputComponent),
    vol.Required(CONF_FREQUENCY): vol.All(cv.frequency,
                                          vol.Range(min=23.84, max=1525.88)),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_pca9685_component(config.get(CONF_FREQUENCY))
    pca9685 = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(pca9685.set_address(config[CONF_ADDRESS]))
    setup_component(pca9685, config)


BUILD_FLAGS = '-DUSE_PCA9685_OUTPUT'
