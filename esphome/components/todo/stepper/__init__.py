from esphome.automation import ACTION_REGISTRY
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ACCELERATION, CONF_DECELERATION, CONF_ID, CONF_MAX_SPEED, \
    CONF_POSITION, CONF_TARGET
from esphome.core import CORE
PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

# pylint: disable=invalid-name
stepper_ns = esphome_ns.namespace('stepper')
Stepper = stepper_ns.class_('Stepper')

SetTargetAction = stepper_ns.class_('SetTargetAction', Action)
ReportPositionAction = stepper_ns.class_('ReportPositionAction', Action)


def validate_acceleration(value):
    value = cv.string(value)
    for suffix in ('steps/s^2', 'steps/s*s', 'steps/s/s', 'steps/ss', 'steps/(s*s)'):
        if value.endswith(suffix):
            value = value[:-len(suffix)]

    if value == 'inf':
        return 1e6

    try:
        value = float(value)
    except ValueError:
        raise cv.Invalid("Expected acceleration as floating point number, got {}".format(value))

    if value <= 0:
        raise cv.Invalid("Acceleration must be larger than 0 steps/s^2!")

    return value


def validate_speed(value):
    value = cv.string(value)
    for suffix in ('steps/s', 'steps/s'):
        if value.endswith(suffix):
            value = value[:-len(suffix)]

    if value == 'inf':
        return 1e6

    try:
        value = float(value)
    except ValueError:
        raise cv.Invalid("Expected speed as floating point number, got {}".format(value))

    if value <= 0:
        raise cv.Invalid("Speed must be larger than 0 steps/s!")

    return value


STEPPER_SCHEMA = cv.Schema({
    cv.Required(CONF_MAX_SPEED): validate_speed,
    cv.Optional(CONF_ACCELERATION): validate_acceleration,
    cv.Optional(CONF_DECELERATION): validate_acceleration,
})

STEPPER_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(STEPPER_SCHEMA.schema)


def setup_stepper_core_(stepper_var, config):
    if CONF_ACCELERATION in config:
        cg.add(stepper_var.set_acceleration(config[CONF_ACCELERATION]))
    if CONF_DECELERATION in config:
        cg.add(stepper_var.set_deceleration(config[CONF_DECELERATION]))
    if CONF_MAX_SPEED in config:
        cg.add(stepper_var.set_max_speed(config[CONF_MAX_SPEED]))


def setup_stepper(stepper_var, config):
    CORE.add_job(setup_stepper_core_, stepper_var, config)


BUILD_FLAGS = '-DUSE_STEPPER'

CONF_STEPPER_SET_TARGET = 'stepper.set_target'
STEPPER_SET_TARGET_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(Stepper),
    cv.Required(CONF_TARGET): cv.templatable(cv.int_),
})


@ACTION_REGISTRY.register(CONF_STEPPER_SET_TARGET, STEPPER_SET_TARGET_ACTION_SCHEMA)
def stepper_set_target_to_code(config, action_id, template_arg, args):
    var = yield get_variable(config[CONF_ID])
    rhs = var.make_set_target_action(template_arg)
    type = SetTargetAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    template_ = yield templatable(config[CONF_TARGET], args, int32)
    cg.add(action.set_target(template_))
    yield action


CONF_STEPPER_REPORT_POSITION = 'stepper.report_position'
STEPPER_REPORT_POSITION_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(Stepper),
    cv.Required(CONF_POSITION): cv.templatable(cv.int_),
})


@ACTION_REGISTRY.register(CONF_STEPPER_REPORT_POSITION, STEPPER_REPORT_POSITION_ACTION_SCHEMA)
def stepper_report_position_to_code(config, action_id, template_arg, args):
    var = yield get_variable(config[CONF_ID])
    rhs = var.make_report_position_action(template_arg)
    type = ReportPositionAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    template_ = yield templatable(config[CONF_POSITION], args, int32)
    cg.add(action.set_position(template_))
    yield action
