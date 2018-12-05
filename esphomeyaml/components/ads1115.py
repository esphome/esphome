import voluptuous as vol

from esphomeyaml.components import i2c, sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_ID
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Component

DEPENDENCIES = ['i2c']
MULTI_CONF = True

ADS1115Component = sensor.sensor_ns.class_('ADS1115Component', Component, i2c.I2CDevice)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ADS1115Component),
    vol.Required(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_ads1115_component(config[CONF_ADDRESS])
    var = Pvariable(config[CONF_ID], rhs)
    setup_component(var, config)


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'
