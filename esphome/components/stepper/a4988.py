import voluptuous as vol

from esphome import pins
from esphome.components import stepper
import esphome.config_validation as cv
from esphome.const import CONF_DIR_PIN, CONF_ID, CONF_SLEEP_PIN, CONF_STEP_PIN
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

A4988 = stepper.stepper_ns.class_('A4988', stepper.Stepper, Component)

PLATFORM_SCHEMA = stepper.STEPPER_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(A4988),
    vol.Required(CONF_STEP_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_DIR_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_SLEEP_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for step_pin in gpio_output_pin_expression(config[CONF_STEP_PIN]):
        yield
    for dir_pin in gpio_output_pin_expression(config[CONF_DIR_PIN]):
        yield
    rhs = App.make_a4988(step_pin, dir_pin)
    a4988 = Pvariable(config[CONF_ID], rhs)

    if CONF_SLEEP_PIN in config:
        for sleep_pin in gpio_output_pin_expression(config[CONF_SLEEP_PIN]):
            yield
        add(a4988.set_sleep_pin(sleep_pin))

    stepper.setup_stepper(a4988, config)
    setup_component(a4988, config)


BUILD_FLAGS = '-DUSE_A4988'
