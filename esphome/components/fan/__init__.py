from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_OFF_SPEED_CYCLE,
    CONF_ON_DIRECTION_SET,
    CONF_ON_OSCILLATING_SET,
    CONF_ON_PRESET_SET,
    CONF_ON_SPEED_SET,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_OSCILLATING,
    CONF_OSCILLATION_COMMAND_TOPIC,
    CONF_OSCILLATION_STATE_TOPIC,
    CONF_RESTORE_MODE,
    CONF_SPEED,
    CONF_SPEED_COMMAND_TOPIC,
    CONF_SPEED_LEVEL_COMMAND_TOPIC,
    CONF_SPEED_LEVEL_STATE_TOPIC,
    CONF_SPEED_STATE_TOPIC,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
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

FanStateTrigger = fan_ns.class_(
    "FanStateTrigger", automation.Trigger.template(Fan.operator("ptr"))
)
FanTurnOnTrigger = fan_ns.class_("FanTurnOnTrigger", automation.Trigger.template())
FanTurnOffTrigger = fan_ns.class_("FanTurnOffTrigger", automation.Trigger.template())
FanDirectionSetTrigger = fan_ns.class_(
    "FanDirectionSetTrigger", automation.Trigger.template(FanDirection)
)
FanOscillatingSetTrigger = fan_ns.class_(
    "FanOscillatingSetTrigger", automation.Trigger.template(cg.bool_)
)
FanSpeedSetTrigger = fan_ns.class_(
    "FanSpeedSetTrigger", automation.Trigger.template(cg.int_)
)
FanPresetSetTrigger = fan_ns.class_(
    "FanPresetSetTrigger", automation.Trigger.template(cg.std_string)
)

FanIsOnCondition = fan_ns.class_("FanIsOnCondition", automation.Condition.template())
FanIsOffCondition = fan_ns.class_("FanIsOffCondition", automation.Condition.template())

FAN_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
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
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanStateTrigger),
                }
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
            cv.Optional(CONF_ON_DIRECTION_SET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        FanDirectionSetTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_OSCILLATING_SET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        FanOscillatingSetTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_SPEED_SET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanSpeedSetTrigger),
                }
            ),
            cv.Optional(CONF_ON_PRESET_SET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FanPresetSetTrigger),
                }
            ),
        }
    )
)

_PRESET_MODES_SCHEMA = cv.All(
    cv.ensure_list(cv.string_strict),
    cv.Length(min=1),
)


def validate_preset_modes(value):
    # Check against defined schema
    value = _PRESET_MODES_SCHEMA(value)

    # Ensure preset names are unique
    errors = []
    presets = set()
    for i, preset in enumerate(value):
        # If name does not exist yet add it
        if preset not in presets:
            presets.add(preset)
            continue

        # Otherwise it's an error
        errors.append(
            cv.Invalid(
                f"Found duplicate preset name '{preset}'. Presets must have unique names.",
                [i],
            )
        )

    if errors:
        raise cv.MultipleInvalid(errors)

    return value


async def setup_fan_core_(var, config):
    await setup_entity(var, config)

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if (
            oscillation_state_topic := config.get(CONF_OSCILLATION_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_oscillation_state_topic(oscillation_state_topic))
        if (
            oscillation_command_topic := config.get(CONF_OSCILLATION_COMMAND_TOPIC)
        ) is not None:
            cg.add(
                mqtt_.set_custom_oscillation_command_topic(oscillation_command_topic)
            )
        if (
            speed_level_state_topic := config.get(CONF_SPEED_LEVEL_STATE_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_speed_level_state_topic(speed_level_state_topic))
        if (
            speed_level_command_topic := config.get(CONF_SPEED_LEVEL_COMMAND_TOPIC)
        ) is not None:
            cg.add(
                mqtt_.set_custom_speed_level_command_topic(speed_level_command_topic)
            )
        if (speed_state_topic := config.get(CONF_SPEED_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_speed_state_topic(speed_state_topic))
        if (speed_command_topic := config.get(CONF_SPEED_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_speed_command_topic(speed_command_topic))

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(Fan.operator("ptr"), "x")], conf)
    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DIRECTION_SET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(FanDirection, "x")], conf)
    for conf in config.get(CONF_ON_OSCILLATING_SET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.bool_, "x")], conf)
    for conf in config.get(CONF_ON_SPEED_SET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.int_, "x")], conf)
    for conf in config.get(CONF_ON_PRESET_SET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)


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
    if (oscillating := config.get(CONF_OSCILLATING)) is not None:
        template_ = await cg.templatable(oscillating, args, bool)
        cg.add(var.set_oscillating(template_))
    if (speed := config.get(CONF_SPEED)) is not None:
        template_ = await cg.templatable(speed, args, int)
        cg.add(var.set_speed(template_))
    if (direction := config.get(CONF_DIRECTION)) is not None:
        template_ = await cg.templatable(direction, args, FanDirection)
        cg.add(var.set_direction(template_))
    return var


@automation.register_action(
    "fan.cycle_speed",
    CycleSpeedAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Fan),
            cv.Optional(CONF_OFF_SPEED_CYCLE, default=True): cv.boolean,
        }
    ),
)
async def fan_cycle_speed_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_OFF_SPEED_CYCLE], args, bool)
    cg.add(var.set_no_off_cycle(template_))
    return var


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
