import voluptuous as vol

from esphomeyaml.automation import maybe_simple_id, ACTION_REGISTRY
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_MQTT_ID, CONF_INTERNAL
from esphomeyaml.helpers import Pvariable, esphomelib_ns, setup_mqtt_component, add, \
    TemplateArguments, get_variable

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

cover_ns = esphomelib_ns.namespace('cover')
Cover = cover_ns.Cover
MQTTCoverComponent = cover_ns.MQTTCoverComponent
CoverState = cover_ns.CoverState
COVER_OPEN = cover_ns.COVER_OPEN
COVER_CLOSED = cover_ns.COVER_CLOSED
OpenAction = cover_ns.OpenAction
CloseAction = cover_ns.CloseAction
StopAction = cover_ns.StopAction

COVER_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Cover),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTCoverComponent),
})

COVER_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(COVER_SCHEMA.schema)


def setup_cover_core_(cover_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(cover_var.set_internal(config[CONF_INTERNAL]))
    setup_mqtt_component(mqtt_var, config)


def setup_cover(cover_obj, mqtt_obj, config):
    cover_var = Pvariable(config[CONF_ID], cover_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_cover_core_(cover_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_COVER'

CONF_COVER_OPEN = 'cover.open'
COVER_OPEN_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_COVER_OPEN, COVER_OPEN_ACTION_SCHEMA)
def cover_open_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_open_action(template_arg)
    type = OpenAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_COVER_CLOSE = 'cover.close'
COVER_CLOSE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_COVER_CLOSE, COVER_CLOSE_ACTION_SCHEMA)
def cover_close_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_close_action(template_arg)
    type = CloseAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_COVER_STOP = 'cover.stop'
COVER_STOP_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_COVER_STOP, COVER_STOP_ACTION_SCHEMA)
def cover_stop_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_stop_action(template_arg)
    type = StopAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)
