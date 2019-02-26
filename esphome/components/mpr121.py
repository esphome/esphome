import voluptuous as vol

from esphome.components import i2c, binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['i2c']
MULTI_CONF = True

CONF_MPR121_ID = 'mpr121_id'
MPR121Component = binary_sensor.binary_sensor_ns.class_('MPR121Component', Component, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(MPR121Component),
    vol.Optional(CONF_ADDRESS): cv.i2c_address
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_mpr121(config.get(CONF_ADDRESS))
    var = Pvariable(config[CONF_ID], rhs)

    setup_component(var, config)


BUILD_FLAGS = '-DUSE_MPR121'
