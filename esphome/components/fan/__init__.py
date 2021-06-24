import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_OSCILLATING,
    CONF_OSCILLATION_COMMAND_TOPIC,
    CONF_OSCILLATION_STATE_TOPIC,
    CONF_SPEED,
    CONF_SPEED_COMMAND_TOPIC,
    CONF_SPEED_STATE_TOPIC,
    CONF_NAME,
    CONF_OBJECT_ID,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE, coroutine_with_priority

IS_PLATFORM_COMPONENT = True

fan_ns = cg.esphome_ns.namespace("fan")
FanState = fan_ns.class_("FanState", cg.Nameable, cg.Component)
MakeFan = cg.Application.struct("MakeFan")

# Actions
TurnOnAction = fan_ns.class_("TurnOnAction", automation.Action)
TurnOffAction = fan_ns.class_("TurnOffAction", automation.Action)
ToggleAction = fan_ns.class_("ToggleAction", automation.Action)

FanTurnOnTrigger = fan_ns.class_("FanTurnOnTrigger", automation.Trigger.template())
FanTurnOffTrigger = fan_ns.class_("FanTurnOffTrigger", automation.Trigger.template())

FAN_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(FanState),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTFanComponent),
        cv.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.All(
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
    }
)


async def setup_fan_core_(var, config):
    cg.add(var.set_name(config[CONF_NAME]))
    if CONF_OBJECT_ID in config:
        cg.add(var.set_object_id(config[CONF_OBJECT_ID]))
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))

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


async def register_fan(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_fan(var))
    await cg.register_component(var, config)
    await setup_fan_core_(var, config)


async def create_fan_state(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_fan(var, config)
    return var


FAN_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(FanState),
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
            cv.Required(CONF_ID): cv.use_id(FanState),
            cv.Optional(CONF_OSCILLATING): cv.templatable(cv.boolean),
            cv.Optional(CONF_SPEED): cv.templatable(cv.int_range(1)),
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
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_FAN")
    cg.add_global(fan_ns.using)
