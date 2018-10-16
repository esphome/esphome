import voluptuous as vol

from esphomeyaml.automation import maybe_simple_id, ACTION_REGISTRY
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_MQTT_ID, CONF_OSCILLATION_COMMAND_TOPIC, \
    CONF_OSCILLATION_STATE_TOPIC, CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC, CONF_INTERNAL, \
    CONF_SPEED, CONF_OSCILLATING
from esphomeyaml.helpers import Application, Pvariable, add, esphomelib_ns, setup_mqtt_component, \
    TemplateArguments, get_variable, templatable, bool_

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

fan_ns = esphomelib_ns.namespace('fan')
FanState = fan_ns.FanState
MQTTFanComponent = fan_ns.MQTTFanComponent
MakeFan = Application.MakeFan
TurnOnAction = fan_ns.TurnOnAction
TurnOffAction = fan_ns.TurnOffAction
ToggleAction = fan_ns.ToggleAction
FanSpeed = fan_ns.FanSpeed
FAN_SPEED_OFF = fan_ns.FAN_SPEED_OFF
FAN_SPEED_LOW = fan_ns.FAN_SPEED_LOW
FAN_SPEED_MEDIUM = fan_ns.FAN_SPEED_MEDIUM
FAN_SPEED_HIGH = fan_ns.FAN_SPEED_HIGH

FAN_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(FanState),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTFanComponent),
    vol.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.subscribe_topic,
})

FAN_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(FAN_SCHEMA.schema)


FAN_SPEEDS = {
    'OFF': FAN_SPEED_OFF,
    'LOW': FAN_SPEED_LOW,
    'MEDIUM': FAN_SPEED_MEDIUM,
    'HIGH': FAN_SPEED_HIGH,
}


def validate_fan_speed(value):
    return vol.All(vol.Upper, cv.one_of(*FAN_SPEEDS))(value)


def setup_fan_core_(fan_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(fan_var.set_internal(config[CONF_INTERNAL]))

    if CONF_OSCILLATION_STATE_TOPIC in config:
        add(mqtt_var.set_custom_oscillation_state_topic(config[CONF_OSCILLATION_STATE_TOPIC]))
    if CONF_OSCILLATION_COMMAND_TOPIC in config:
        add(mqtt_var.set_custom_oscillation_command_topic(config[CONF_OSCILLATION_COMMAND_TOPIC]))
    if CONF_SPEED_STATE_TOPIC in config:
        add(mqtt_var.set_custom_speed_state_topic(config[CONF_SPEED_STATE_TOPIC]))
    if CONF_SPEED_COMMAND_TOPIC in config:
        add(mqtt_var.set_custom_speed_command_topic(config[CONF_SPEED_COMMAND_TOPIC]))
    setup_mqtt_component(mqtt_var, config)


def setup_fan(fan_obj, mqtt_obj, config):
    fan_var = Pvariable(config[CONF_ID], fan_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_fan_core_(fan_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_FAN'


CONF_FAN_TOGGLE = 'fan.toggle'
FAN_TOGGLE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_FAN_TOGGLE, FAN_TOGGLE_ACTION_SCHEMA)
def fan_toggle_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_toggle_action(template_arg)
    type = ToggleAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_FAN_TURN_OFF = 'fan.turn_off'
FAN_TURN_OFF_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_FAN_TURN_OFF, FAN_TURN_OFF_ACTION_SCHEMA)
def fan_turn_off_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_off_action(template_arg)
    type = TurnOffAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_FAN_TURN_ON = 'fan.turn_on'
FAN_TURN_ON_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
    vol.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
    vol.Optional(CONF_SPEED): cv.templatable(validate_fan_speed),
})


@ACTION_REGISTRY.register(CONF_FAN_TURN_ON, FAN_TURN_ON_ACTION_SCHEMA)
def fan_turn_on_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_on_action(template_arg)
    type = TurnOnAction.template(arg_type)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_OSCILLATING in config:
        for template_ in templatable(config[CONF_OSCILLATING], arg_type, bool_):
            yield None
        add(action.set_oscillating(template_))
    if CONF_SPEED in config:
        for template_ in templatable(config[CONF_SPEED], arg_type, FanSpeed):
            yield None
        add(action.set_speed(template_))
    yield action
