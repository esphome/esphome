import voluptuous as vol

from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SCL, CONF_SDO
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component
from esphome.cpp_helpers import gpio_output_pin_expression
from esphome.pins import gpio_input_pullup_pin_schema

MULTI_CONF = True

CONF_TTP229_BSF_ID = 'ttp229_bsf_id'
TTP229BSFComponent = binary_sensor.binary_sensor_ns.class_('TTP229BSFComponent', Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(TTP229BSFComponent),
    vol.Required(CONF_SDO): gpio_input_pullup_pin_schema,
    vol.Required(CONF_SCL): gpio_input_pullup_pin_schema,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for sdo_pin in gpio_output_pin_expression(config[CONF_SDO]):
        yield
    for scl_pin in gpio_output_pin_expression(config[CONF_SCL]):
        yield
    rhs = App.make_ttp229_bsf(sdo_pin, scl_pin)
    var = Pvariable(config[CONF_ID], rhs)

    setup_component(var, config)


BUILD_FLAGS = '-DUSE_TTP229_BSF'