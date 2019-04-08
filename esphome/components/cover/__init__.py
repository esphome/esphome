import voluptuous as vol

from esphome.automation import ACTION_REGISTRY, maybe_simple_id, Condition
from esphome.components import mqtt
from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_MQTT_ID, CONF_DEVICE_CLASS, CONF_STATE, \
    CONF_POSITION, CONF_TILT, CONF_STOP
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable, templatable
from esphome.cpp_types import Action, Nameable, esphome_ns, App

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

DEVICE_CLASSES = [
    '', 'awning', 'blind', 'curtain', 'damper', 'door', 'garage',
    'shade', 'shutter', 'window'
]

cover_ns = esphome_ns.namespace('cover')

Cover = cover_ns.class_('Cover', Nameable)
MQTTCoverComponent = cover_ns.class_('MQTTCoverComponent', mqtt.MQTTComponent)

COVER_OPEN = cover_ns.COVER_OPEN
COVER_CLOSED = cover_ns.COVER_CLOSED

COVER_STATES = {
    'OPEN': COVER_OPEN,
    'CLOSED': COVER_CLOSED,
}
validate_cover_state = cv.one_of(*COVER_STATES, upper=True)

CoverOperation = cover_ns.enum('CoverOperation')
COVER_OPERATIONS = {
    'IDLE': CoverOperation.COVER_OPERATION_IDLE,
    'OPENING': CoverOperation.COVER_OPERATION_OPENING,
    'CLOSING': CoverOperation.COVER_OPERATION_CLOSING,
}
validate_cover_operation = cv.one_of(*COVER_OPERATIONS, upper=True)

# Actions
OpenAction = cover_ns.class_('OpenAction', Action)
CloseAction = cover_ns.class_('CloseAction', Action)
StopAction = cover_ns.class_('StopAction', Action)
ControlAction = cover_ns.class_('ControlAction', Action)
CoverIsOpenCondition = cover_ns.class_('CoverIsOpenCondition', Condition)
CoverIsClosedCondition = cover_ns.class_('CoverIsClosedCondition', Condition)

COVER_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Cover),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTCoverComponent),
    vol.Optional(CONF_DEVICE_CLASS): cv.one_of(*DEVICE_CLASSES, lower=True),
})

COVER_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(COVER_SCHEMA.schema)


def setup_cover_core_(cover_var, config):
    if CONF_INTERNAL in config:
        add(cover_var.set_internal(config[CONF_INTERNAL]))
    if CONF_DEVICE_CLASS in config:
        add(cover_var.set_device_class(config[CONF_DEVICE_CLASS]))
    setup_mqtt_component(cover_var.Pget_mqtt(), config)


def setup_cover(cover_obj, config):
    CORE.add_job(setup_cover_core_, cover_obj, config)


def register_cover(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    add(App.register_cover(var))
    CORE.add_job(setup_cover_core_, var, config)


BUILD_FLAGS = '-DUSE_COVER'

CONF_COVER_OPEN = 'cover.open'
COVER_OPEN_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Cover),
})


@ACTION_REGISTRY.register(CONF_COVER_OPEN, COVER_OPEN_ACTION_SCHEMA)
def cover_open_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = OpenAction.template(template_arg)
    rhs = type.new(var)
    yield Pvariable(action_id, rhs, type=type)


CONF_COVER_CLOSE = 'cover.close'
COVER_CLOSE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Cover),
})


@ACTION_REGISTRY.register(CONF_COVER_CLOSE, COVER_CLOSE_ACTION_SCHEMA)
def cover_close_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = CloseAction.template(template_arg)
    rhs = type.new(var)
    yield Pvariable(action_id, rhs, type=type)


CONF_COVER_STOP = 'cover.stop'
COVER_STOP_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Cover),
})


@ACTION_REGISTRY.register(CONF_COVER_STOP, COVER_STOP_ACTION_SCHEMA)
def cover_stop_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = StopAction.template(template_arg)
    rhs = type.new(var)
    yield Pvariable(action_id, rhs, type=type)


CONF_COVER_CONTROL = 'cover.control'
COVER_CONTROL_ACTION_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(Cover),
    vol.Optional(CONF_STOP): cv.templatable(cv.boolean),
    vol.Exclusive(CONF_STATE, 'pos'): cv.templatable(cv.one_of(*COVER_STATES)),
    vol.Exclusive(CONF_POSITION, 'pos'): cv.templatable(cv.percentage),
    vol.Optional(CONF_TILT): cv.templatable(cv.percentage),
})


@ACTION_REGISTRY.register(CONF_COVER_CONTROL, COVER_CONTROL_ACTION_SCHEMA)
def cover_control_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = StopAction.template(template_arg)
    rhs = type.new(var)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_STOP in config:
        for template_ in templatable(config[CONF_STOP], args, bool):
            yield None
        add(action.set_stop(template_))
    if CONF_STATE in config:
        for template_ in templatable(config[CONF_STATE], args, float,
                                     to_exp=COVER_STATES):
            yield None
        add(action.set_position(template_))
    if CONF_POSITION in config:
        for template_ in templatable(config[CONF_POSITION], args, float):
            yield None
        add(action.set_position(template_))
    if CONF_TILT in config:
        for template_ in templatable(config[CONF_TILT], args, float):
            yield None
        add(action.set_tilt(template_))
    yield action
