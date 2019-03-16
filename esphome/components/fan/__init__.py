import voluptuous as vol

from esphome.automation import ACTION_REGISTRY, maybe_simple_id
from esphome.components import mqtt
from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERNAL, CONF_MQTT_ID, CONF_OSCILLATING, \
    CONF_OSCILLATION_COMMAND_TOPIC, CONF_OSCILLATION_STATE_TOPIC, CONF_SPEED, \
    CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable, templatable
from esphome.cpp_types import Action, Application, Component, Nameable, bool_, esphome_ns
from esphome.py_compat import string_types

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

fan_ns = esphome_ns.namespace('fan')
FanState = fan_ns.class_('FanState', Nameable, Component)
MQTTFanComponent = fan_ns.class_('MQTTFanComponent', mqtt.MQTTComponent)
MakeFan = Application.struct('MakeFan')

# Actions
TurnOnAction = fan_ns.class_('TurnOnAction', Action)
TurnOffAction = fan_ns.class_('TurnOffAction', Action)
ToggleAction = fan_ns.class_('ToggleAction', Action)

FanSpeed = fan_ns.enum('FanSpeed')
FAN_SPEED_OFF = FanSpeed.FAN_SPEED_OFF
FAN_SPEED_LOW = FanSpeed.FAN_SPEED_LOW
FAN_SPEED_MEDIUM = FanSpeed.FAN_SPEED_MEDIUM
FAN_SPEED_HIGH = FanSpeed.FAN_SPEED_HIGH

FAN_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(FanState),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTFanComponent),
    vol.Optional(CONF_OSCILLATION_STATE_TOPIC): vol.All(cv.requires_component('mqtt'),
                                                        cv.publish_topic),
    vol.Optional(CONF_OSCILLATION_COMMAND_TOPIC): vol.All(cv.requires_component('mqtt'),
                                                          cv.subscribe_topic),
})

FAN_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(FAN_SCHEMA.schema)

FAN_SPEEDS = {
    'OFF': FAN_SPEED_OFF,
    'LOW': FAN_SPEED_LOW,
    'MEDIUM': FAN_SPEED_MEDIUM,
    'HIGH': FAN_SPEED_HIGH,
}


def setup_fan_core_(fan_var, config):
    if CONF_INTERNAL in config:
        add(fan_var.set_internal(config[CONF_INTERNAL]))

    mqtt_ = fan_var.Pget_mqtt()
    if CONF_OSCILLATION_STATE_TOPIC in config:
        add(mqtt_.set_custom_oscillation_state_topic(config[CONF_OSCILLATION_STATE_TOPIC]))
    if CONF_OSCILLATION_COMMAND_TOPIC in config:
        add(mqtt_.set_custom_oscillation_command_topic(config[CONF_OSCILLATION_COMMAND_TOPIC]))
    if CONF_SPEED_STATE_TOPIC in config:
        add(mqtt_.set_custom_speed_state_topic(config[CONF_SPEED_STATE_TOPIC]))
    if CONF_SPEED_COMMAND_TOPIC in config:
        add(mqtt_.set_custom_speed_command_topic(config[CONF_SPEED_COMMAND_TOPIC]))
    setup_mqtt_component(mqtt_, config)


def setup_fan(fan_obj, config):
    fan_var = Pvariable(config[CONF_ID], fan_obj, has_side_effects=False)
    CORE.add_job(setup_fan_core_, fan_var, config)


BUILD_FLAGS = '-DUSE_FAN'

CONF_FAN_TOGGLE = 'fan.toggle'
FAN_TOGGLE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(FanState),
})


@ACTION_REGISTRY.register(CONF_FAN_TOGGLE, FAN_TOGGLE_ACTION_SCHEMA)
def fan_toggle_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_toggle_action(template_arg)
    type = ToggleAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


CONF_FAN_TURN_OFF = 'fan.turn_off'
FAN_TURN_OFF_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(FanState),
})


@ACTION_REGISTRY.register(CONF_FAN_TURN_OFF, FAN_TURN_OFF_ACTION_SCHEMA)
def fan_turn_off_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_off_action(template_arg)
    type = TurnOffAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


CONF_FAN_TURN_ON = 'fan.turn_on'
FAN_TURN_ON_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(FanState),
    vol.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
    vol.Optional(CONF_SPEED): cv.templatable(cv.one_of(*FAN_SPEEDS, upper=True)),
})


@ACTION_REGISTRY.register(CONF_FAN_TURN_ON, FAN_TURN_ON_ACTION_SCHEMA)
def fan_turn_on_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_on_action(template_arg)
    type = TurnOnAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_OSCILLATING in config:
        for template_ in templatable(config[CONF_OSCILLATING], args, bool_):
            yield None
        add(action.set_oscillating(template_))
    if CONF_SPEED in config:
        for template_ in templatable(config[CONF_SPEED], args, FanSpeed):
            yield None
        if isinstance(template_, string_types):
            template_ = FAN_SPEEDS[template_]
        add(action.set_speed(template_))
    yield action
