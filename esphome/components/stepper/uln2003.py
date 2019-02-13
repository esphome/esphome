import voluptuous as vol

from esphome import pins
from esphome.components import stepper
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN_A, CONF_PIN_B, CONF_PIN_C, CONF_PIN_D, \
    CONF_SLEEP_WHEN_DONE, CONF_STEP_MODE
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

ULN2003StepMode = stepper.stepper_ns.enum('ULN2003StepMode')

STEP_MODES = {
    'FULL_STEP': ULN2003StepMode.ULN2003_STEP_MODE_FULL_STEP,
    'HALF_STEP': ULN2003StepMode.ULN2003_STEP_MODE_HALF_STEP,
    'WAVE_DRIVE': ULN2003StepMode.ULN2003_STEP_MODE_WAVE_DRIVE,
}

ULN2003 = stepper.stepper_ns.class_('ULN2003', stepper.Stepper, Component)

PLATFORM_SCHEMA = stepper.STEPPER_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(ULN2003),
    vol.Required(CONF_PIN_A): pins.gpio_output_pin_schema,
    vol.Required(CONF_PIN_B): pins.gpio_output_pin_schema,
    vol.Required(CONF_PIN_C): pins.gpio_output_pin_schema,
    vol.Required(CONF_PIN_D): pins.gpio_output_pin_schema,
    vol.Optional(CONF_SLEEP_WHEN_DONE): cv.boolean,
    vol.Optional(CONF_STEP_MODE): cv.one_of(*STEP_MODES, upper=True, space='_')
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin_a in gpio_output_pin_expression(config[CONF_PIN_A]):
        yield
    for pin_b in gpio_output_pin_expression(config[CONF_PIN_B]):
        yield
    for pin_c in gpio_output_pin_expression(config[CONF_PIN_C]):
        yield
    for pin_d in gpio_output_pin_expression(config[CONF_PIN_D]):
        yield
    rhs = App.make_uln2003(pin_a, pin_b, pin_c, pin_d)
    uln = Pvariable(config[CONF_ID], rhs)

    if CONF_SLEEP_WHEN_DONE in config:
        add(uln.set_sleep_when_done(config[CONF_SLEEP_WHEN_DONE]))

    if CONF_STEP_MODE in config:
        add(uln.set_step_mode(STEP_MODES[config[CONF_STEP_MODE]]))

    stepper.setup_stepper(uln, config)
    setup_component(uln, config)


BUILD_FLAGS = '-DUSE_ULN2003'
