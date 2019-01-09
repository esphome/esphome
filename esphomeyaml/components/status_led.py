import voluptuous as vol

from esphomeyaml import config_validation as cv, pins
from esphomeyaml.const import CONF_ID, CONF_PIN
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import gpio_output_pin_expression, setup_component
from esphomeyaml.cpp_types import App, Component, esphomelib_ns

StatusLEDComponent = esphomelib_ns.class_('StatusLEDComponent', Component)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(StatusLEDComponent),
    vol.Optional(CONF_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_output_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_status_led(pin)
    var = Pvariable(config[CONF_ID], rhs)

    setup_component(var, config)


BUILD_FLAGS = '-DUSE_STATUS_LED'
