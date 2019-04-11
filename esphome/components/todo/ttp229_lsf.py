from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import register_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['i2c']

CONF_TTP229_ID = 'ttp229_id'
TTP229LSFComponent = binary_sensor.binary_sensor_ns.class_('TTP229LSFComponent', Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(TTP229LSFComponent),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_ttp229_lsf()
    var = Pvariable(config[CONF_ID], rhs)

    register_component(var, config)


BUILD_FLAGS = '-DUSE_TTP229_LSF'
