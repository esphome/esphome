import voluptuous as vol

from esphome.automation import ACTION_REGISTRY
from esphome.components.output import FloatOutput
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_IDLE_LEVEL, CONF_MAX_LEVEL, CONF_MIN_LEVEL, CONF_OUTPUT, \
    CONF_LEVEL
from esphome.cpp_generator import Pvariable, add, get_variable, templatable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component, esphome_ns, Action, float_

Servo = esphome_ns.class_('Servo', Component)
ServoWriteAction = esphome_ns.class_('ServoWriteAction', Action)

MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(Servo),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(FloatOutput),
    vol.Optional(CONF_MIN_LEVEL, default='3%'): cv.percentage,
    vol.Optional(CONF_IDLE_LEVEL, default='7.5%'): cv.percentage,
    vol.Optional(CONF_MAX_LEVEL, default='12%'): cv.percentage,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for out in get_variable(config[CONF_OUTPUT]):
        yield

    rhs = App.register_component(Servo.new(out))
    servo = Pvariable(config[CONF_ID], rhs)

    add(servo.set_min_level(config[CONF_MIN_LEVEL]))
    add(servo.set_idle_level(config[CONF_IDLE_LEVEL]))
    add(servo.set_max_level(config[CONF_MAX_LEVEL]))

    setup_component(servo, config)


BUILD_FLAGS = '-DUSE_SERVO'

CONF_SERVO_WRITE = 'servo.write'
SERVO_WRITE_ACTION_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(Servo),
    vol.Required(CONF_LEVEL): cv.templatable(cv.possibly_negative_percentage),
})


@ACTION_REGISTRY.register(CONF_SERVO_WRITE, SERVO_WRITE_ACTION_SCHEMA)
def servo_write_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = ServoWriteAction.new(template_arg, var)
    type = ServoWriteAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_LEVEL], args, float_):
        yield None
    add(action.set_value(template_))
    yield action
