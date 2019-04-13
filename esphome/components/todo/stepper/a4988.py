from esphome import pins
from esphome.components import stepper
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_DIR_PIN, CONF_ID, CONF_SLEEP_PIN, CONF_STEP_PIN


A4988 = stepper.stepper_ns.class_('A4988', stepper.Stepper, Component)

PLATFORM_SCHEMA = stepper.STEPPER_PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_variable_id(A4988),
    cv.Required(CONF_STEP_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DIR_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_SLEEP_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    step_pin = yield gpio_output_pin_expression(config[CONF_STEP_PIN])
    dir_pin = yield gpio_output_pin_expression(config[CONF_DIR_PIN])
    rhs = App.make_a4988(step_pin, dir_pin)
    a4988 = Pvariable(config[CONF_ID], rhs)

    if CONF_SLEEP_PIN in config:
        sleep_pin = yield gpio_output_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(a4988.set_sleep_pin(sleep_pin))

    stepper.setup_stepper(a4988, config)
    register_component(a4988, config)


BUILD_FLAGS = '-DUSE_A4988'
