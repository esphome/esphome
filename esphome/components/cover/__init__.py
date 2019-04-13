import esphome.codegen as cg
# from esphome.components import mqtt
# from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.automation import ACTION_REGISTRY, maybe_simple_id, Condition
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_DEVICE_CLASS, CONF_STATE, \
    CONF_POSITION, CONF_TILT, CONF_STOP
from esphome.core import CORE, coroutine

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

DEVICE_CLASSES = [
    '', 'awning', 'blind', 'curtain', 'damper', 'door', 'garage',
    'shade', 'shutter', 'window'
]

cover_ns = cg.esphome_ns.namespace('cover')

Cover = cover_ns.class_('Cover', cg.Nameable)
# MQTTCoverComponent = cover_ns.class_('MQTTCoverComponent', mqtt.MQTTComponent)

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
OpenAction = cover_ns.class_('OpenAction', cg.Action)
CloseAction = cover_ns.class_('CloseAction', cg.Action)
StopAction = cover_ns.class_('StopAction', cg.Action)
ControlAction = cover_ns.class_('ControlAction', cg.Action)
CoverIsOpenCondition = cover_ns.class_('CoverIsOpenCondition', Condition)
CoverIsClosedCondition = cover_ns.class_('CoverIsClosedCondition', Condition)

COVER_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Cover),
    # cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTCoverComponent),
    cv.Optional(CONF_DEVICE_CLASS): cv.one_of(*DEVICE_CLASSES, lower=True),
})

COVER_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(COVER_SCHEMA.schema)


@coroutine
def setup_cover_core_(cover_var, config):
    if CONF_INTERNAL in config:
        cg.add(cover_var.set_internal(config[CONF_INTERNAL]))
    if CONF_DEVICE_CLASS in config:
        cg.add(cover_var.set_device_class(config[CONF_DEVICE_CLASS]))
    # setup_mqtt_component(cover_var.Pget_mqtt(), config)


@coroutine
def register_cover(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_cover(var))
    yield setup_cover_core_(var, config)


COVER_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_variable_id(Cover),
})


@ACTION_REGISTRY.register('cover.open', COVER_ACTION_SCHEMA)
def cover_open_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = OpenAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('cover.close', COVER_ACTION_SCHEMA)
def cover_close_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = CloseAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('cover.stop', COVER_ACTION_SCHEMA)
def cover_stop_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = StopAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


COVER_CONTROL_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_variable_id(Cover),
    cv.Optional(CONF_STOP): cv.templatable(cv.boolean),
    cv.Exclusive(CONF_STATE, 'pos'): cv.templatable(cv.one_of(*COVER_STATES)),
    cv.Exclusive(CONF_POSITION, 'pos'): cv.templatable(cv.percentage),
    cv.Optional(CONF_TILT): cv.templatable(cv.percentage),
})


@ACTION_REGISTRY.register('cover.control', COVER_CONTROL_ACTION_SCHEMA)
def cover_control_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = StopAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    if CONF_STOP in config:
        template_ = yield cg.templatable(config[CONF_STOP], args, bool)
        cg.add(action.set_stop(template_))
    if CONF_STATE in config:
        template_ = yield cg.templatable(config[CONF_STATE], args, float,
                                         to_exp=COVER_STATES)
        cg.add(action.set_position(template_))
    if CONF_POSITION in config:
        template_ = yield cg.templatable(config[CONF_POSITION], args, float)
        cg.add(action.set_position(template_))
    if CONF_TILT in config:
        template_ = yield cg.templatable(config[CONF_TILT], args, float)
        cg.add(action.set_tilt(template_))
    yield action


def to_code(config):
    cg.add_define('USE_COVER')
    cg.add_global(cover_ns.using)
