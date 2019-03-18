import voluptuous as vol

from esphome import pins
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_SCL, CONF_SDO
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

MULTI_CONF = True

CONF_TTP229_ID = 'ttp229_id'
TTP229Component = binary_sensor.binary_sensor_ns.class_('TTP229Component', Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(TTP229Component),
    vol.Required(CONF_SCL): pins.input_pin,
    vol.Required(CONF_SDO): pins.input_pin,
    vol.Optional(CONF_ADDRESS, default='0x5A'): cv.i2c_address
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_ttp229(config[CONF_SCL], config[CONF_SDO], config[CONF_ADDRESS])
    var = Pvariable(config[CONF_ID], rhs)

    setup_component(var, config)


BUILD_FLAGS = '-DUSE_TTP229'
