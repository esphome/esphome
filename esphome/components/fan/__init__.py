import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_MQTT_ID,
    CONF_OSCILLATING,
    CONF_OSCILLATION_COMMAND_TOPIC,
    CONF_OSCILLATION_STATE_TOPIC,
    CONF_SPEED,
    CONF_SPEED_LEVEL_COMMAND_TOPIC,
    CONF_SPEED_LEVEL_STATE_TOPIC,
    CONF_SPEED_COMMAND_TOPIC,
    CONF_SPEED_STATE_TOPIC,
    CONF_ON_SPEED_SET,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_TRIGGER_ID,
    CONF_DIRECTION,
    CONF_RESTORE_MODE,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

IS_PLATFORM_COMPONENT = True

fan_ns = cg.esphome_ns.namespace("fan")
Fan = fan_ns.class_("Fan", cg.EntityBase)
FanState = fan_ns.class_("Fan", Fan, cg.Component)

FanDirection = fan_ns.enum("FanDirection", is_class=True)
FAN_DIRECTION_ENUM = {
    "FORWARD": FanDirection.FORWARD,
    "REVERSE": FanDirection.REVERSE,
}

FanRestoreMode = fan_ns.enum("FanRestoreMode", is_class=True)
RESTORE_MODES = {
    "NO_RESTORE": FanRestoreMode.NO_RESTORE,
    "ALWAYS_OFF": FanRestoreMode.ALWAYS_OFF,
    "ALWAYS_ON": FanRestoreMode.ALWAYS_ON,
    "RESTORE_DEFAULT_OFF": FanRestoreMode.RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": FanRestoreMode.RESTORE_DEFAULT_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": FanRestoreMode.RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": FanRestoreMode.RESTORE_INVERTED_DEFAULT_ON,
}

# Actions
TurnOnAction = fan_ns.class_("TurnOnAction", automation.Action)
TurnOffAction = fan_ns.class_("TurnOffAction", automation.Action)
ToggleAction = fan_ns.class_("ToggleAction", automation.Action)
CycleSpeedAction = fan_ns.class_("CycleSpeedAction", automation.Action)

FanTurnOnTrigger = fan_ns.class_("FanTurnOnTrigger", automation.Trigger.template())
FanTurnOffTrigger = fan_ns.class_("FanTurnOffTrigger", automation.Trigger.template())
FanSpeedSetTrigger = fan_ns.class_("FanSpeedSetTrigger", automation.Trigger.template())

FanIsOnCondition = fan_ns.class_("FanIsOnCondition", automation.Condition.template())
FanIsOffCondition = fan_ns.class_("FanIsOffCondition", automation.Condition.template())

FAN_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(Fan),
        cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTFanComponent),
        cv.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_SPEED_LEVEL_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_SPEED_LEVEL_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_SPEED_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_SPEED_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanTurnOnTrigger),
            }
        ),
        cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanTurnOffTrigger),
            }
        ),
        cv.Optional(CONF_ON_SPEED_SET): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanSpeedSetTrigger),
            }
        ),
    }
)


async def setup_fan_core_(var, config):
    await setup_entity(var, config)

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if CONF_OSCILLATION_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_oscillation_state_topic(
                    config[CONF_OSCILLATION_STATE_TOPIC]
                )
            )
        if CONF_OSCILLATION_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_oscillation_command_topic(
                    config[CONF_OSCILLATION_COMMAND_TOPIC]
                )
            )
        if CONF_SPEED_LEVEL_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_speed_level_state_topic(
                    config[CONF_SPEED_LEVEL_STATE_TOPIC]
                )
            )
        if CONF_SPEED_LEVEL_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_speed_level_command_topic(
                    config[CONF_SPEED_LEVEL_COMMAND_TOPIC]
                )
            )
        if CONF_SPEED_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_speed_state_topic(config[CONF_SPEED_STATE_TOPIC]))
        if CONF_SPEED_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_speed_command_topic(config[CONF_SPEED_COMMAND_TOPIC])
            )

    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_SPEED_SET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_fan(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_fan(var))
    await setup_fan_core_(var, config)


async def create_fan_state(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_fan(var, config)
    await cg.register_component(var, config)
    return var


FAN_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Fan),
    }
)


@automation.register_action("fan.toggle", ToggleAction, FAN_ACTION_SCHEMA)
async def fan_toggle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("fan.turn_off", TurnOffAction, FAN_ACTION_SCHEMA)
async def fan_turn_off_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "fan.turn_on",
    TurnOnAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
            cv.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
            cv.Optional(CONF_SPEED): cv.templatable(cv.int_range(1)),
            cv.Optional(CONF_DIRECTION): cv.templatable(
                cv.enum(FAN_DIRECTION_ENUM, upper=True)
            ),
        }
    ),
)
async def fan_turn_on_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_OSCILLATING in config:
        template_ = await cg.templatable(config[CONF_OSCILLATING], args, bool)
        cg.add(var.set_oscillating(template_))
    if CONF_SPEED in config:
        template_ = await cg.templatable(config[CONF_SPEED], args, int)
        cg.add(var.set_speed(template_))
    if CONF_DIRECTION in config:
        template_ = await cg.templatable(config[CONF_DIRECTION], args, FanDirection)
        cg.add(var.set_direction(template_))
    return var


@automation.register_action("fan.cycle_speed", CycleSpeedAction, FAN_ACTION_SCHEMA)
async def fan_cycle_speed_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "fan.is_on",
    FanIsOnCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
        }
    ),
)
@automation.register_condition(
    "fan.is_off",
    FanIsOffCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
        }
    ),
)
async def fan_is_on_off_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_FAN")
    cg.add_global(fan_ns.using)
