import voluptuous as vol

from esphomeyaml.automation import maybe_simple_id, ACTION_REGISTRY, CONDITION_REGISTRY, Condition
from esphomeyaml.components import mqtt
from esphomeyaml.components.mqtt import setup_mqtt_component
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ICON, CONF_ID, CONF_INVERTED, CONF_MQTT_ID, CONF_INTERNAL, \
    CONF_OPTIMISTIC
from esphomeyaml.cpp_generator import add, Pvariable, get_variable
from esphomeyaml.cpp_types import esphomelib_ns, Nameable, Action, App

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

switch_ns = esphomelib_ns.namespace('switch_')
Switch = switch_ns.class_('Switch', Nameable)
SwitchPtr = Switch.operator('ptr')
MQTTSwitchComponent = switch_ns.class_('MQTTSwitchComponent', mqtt.MQTTComponent)

ToggleAction = switch_ns.class_('ToggleAction', Action)
TurnOffAction = switch_ns.class_('TurnOffAction', Action)
TurnOnAction = switch_ns.class_('TurnOnAction', Action)

SwitchCondition = switch_ns.class_('SwitchCondition', Condition)

SWITCH_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTSwitchComponent),
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_INVERTED): cv.boolean,
})

SWITCH_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(SWITCH_SCHEMA.schema)


def setup_switch_core_(switch_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(switch_var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        add(switch_var.set_icon(config[CONF_ICON]))
    if CONF_INVERTED in config:
        add(switch_var.set_inverted(config[CONF_INVERTED]))

    setup_mqtt_component(mqtt_var, config)


def setup_switch(switch_obj, mqtt_obj, config):
    switch_var = Pvariable(config[CONF_ID], switch_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_switch_core_(switch_var, mqtt_var, config)


def register_switch(var, config):
    switch_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    rhs = App.register_switch(switch_var)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], rhs, has_side_effects=True)
    setup_switch_core_(switch_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_SWITCH'


CONF_SWITCH_TOGGLE = 'switch.toggle'
SWITCH_TOGGLE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Switch),
})


@ACTION_REGISTRY.register(CONF_SWITCH_TOGGLE, SWITCH_TOGGLE_ACTION_SCHEMA)
def switch_toggle_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_toggle_action(template_arg)
    type = ToggleAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_SWITCH_TURN_OFF = 'switch.turn_off'
SWITCH_TURN_OFF_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Switch),
})


@ACTION_REGISTRY.register(CONF_SWITCH_TURN_OFF, SWITCH_TURN_OFF_ACTION_SCHEMA)
def switch_turn_off_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_off_action(template_arg)
    type = TurnOffAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_SWITCH_TURN_ON = 'switch.turn_on'
SWITCH_TURN_ON_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Switch),
})


@ACTION_REGISTRY.register(CONF_SWITCH_TURN_ON, SWITCH_TURN_ON_ACTION_SCHEMA)
def switch_turn_on_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_on_action(template_arg)
    type = TurnOnAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_SWITCH_IS_ON = 'switch.is_on'
SWITCH_IS_ON_CONDITION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Switch),
})


@CONDITION_REGISTRY.register(CONF_SWITCH_IS_ON, SWITCH_IS_ON_CONDITION_SCHEMA)
def switch_is_on_to_code(config, condition_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_switch_is_on_condition(template_arg)
    type = SwitchCondition.template(arg_type)
    yield Pvariable(condition_id, rhs, type=type)


CONF_SWITCH_IS_OFF = 'switch.is_off'
SWITCH_IS_OFF_CONDITION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(Switch),
})


@CONDITION_REGISTRY.register(CONF_SWITCH_IS_OFF, SWITCH_IS_OFF_CONDITION_SCHEMA)
def switch_is_off_to_code(config, condition_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_switch_is_off_condition(template_arg)
    type = SwitchCondition.template(arg_type)
    yield Pvariable(condition_id, rhs, type=type)


def core_to_hass_config(data, config):
    ret = mqtt.build_hass_config(data, 'switch', config, include_state=True, include_command=True)
    if ret is None:
        return None
    if CONF_ICON in config:
        ret['icon'] = config[CONF_ICON]
    if CONF_OPTIMISTIC in config:
        ret['optimistic'] = config[CONF_OPTIMISTIC]
    return ret
