import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import ACTION_REGISTRY, CONDITION_REGISTRY, Condition, maybe_simple_id
from esphome.components import mqtt
from esphome.const import CONF_ICON, CONF_ID, CONF_INTERNAL, CONF_INVERTED, CONF_ON_TURN_OFF, \
    CONF_ON_TURN_ON, CONF_TRIGGER_ID, CONF_MQTT_ID
from esphome.core import CORE, coroutine

IS_PLATFORM_COMPONENT = True

switch_ns = cg.esphome_ns.namespace('switch_')
Switch = switch_ns.class_('Switch', cg.Nameable)
SwitchPtr = Switch.operator('ptr')

ToggleAction = switch_ns.class_('ToggleAction', cg.Action)
TurnOffAction = switch_ns.class_('TurnOffAction', cg.Action)
TurnOnAction = switch_ns.class_('TurnOnAction', cg.Action)
SwitchPublishAction = switch_ns.class_('SwitchPublishAction', cg.Action)

SwitchCondition = switch_ns.class_('SwitchCondition', Condition)
SwitchTurnOnTrigger = switch_ns.class_('SwitchTurnOnTrigger', cg.Trigger.template())
SwitchTurnOffTrigger = switch_ns.class_('SwitchTurnOffTrigger', cg.Trigger.template())

icon = cv.icon

SWITCH_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.OnlyWith(CONF_MQTT_ID, 'mqtt'): cv.declare_variable_id(mqtt.MQTTSwitchComponent),

    cv.Optional(CONF_ICON): icon,
    cv.Optional(CONF_INVERTED): cv.boolean,
    cv.Optional(CONF_ON_TURN_ON): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SwitchTurnOnTrigger),
    }),
    cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(SwitchTurnOffTrigger),
    }),
})


def setup_switch_core_(var, config):
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))
    if CONF_INVERTED in config:
        cg.add(var.set_inverted(config[CONF_INVERTED]))
    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        yield mqtt.register_mqtt_component(mqtt_, config)


@coroutine
def register_switch(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_switch(var))
    yield setup_switch_core_(var, config)


SWITCH_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_variable_id(Switch),
})


@ACTION_REGISTRY.register('switch.toggle', SWITCH_ACTION_SCHEMA)
def switch_toggle_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = ToggleAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('switch.turn_off', SWITCH_ACTION_SCHEMA)
def switch_turn_off_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = TurnOffAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('switch.turn_on', SWITCH_ACTION_SCHEMA)
def switch_turn_on_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = TurnOnAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@CONDITION_REGISTRY.register('switch.is_on', SWITCH_ACTION_SCHEMA)
def switch_is_on_to_code(config, condition_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = SwitchCondition.template(template_arg)
    rhs = type.new(var, True)
    yield cg.Pvariable(condition_id, rhs, type=type)


@CONDITION_REGISTRY.register('switch.is_off', SWITCH_ACTION_SCHEMA)
def switch_is_off_to_code(config, condition_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = SwitchCondition.template(template_arg)
    rhs = type.new(var, False)
    yield cg.Pvariable(condition_id, rhs, type=type)


def to_code(config):
    cg.add_define('USE_SWITCH')
