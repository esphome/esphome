import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_helpers import setup_entity
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ACTION_STATE_TOPIC,
    CONF_AWAY,
    CONF_AWAY_COMMAND_TOPIC,
    CONF_AWAY_STATE_TOPIC,
    CONF_CURRENT_TEMPERATURE_STATE_TOPIC,
    CONF_CUSTOM_FAN_MODE,
    CONF_CUSTOM_PRESET,
    CONF_FAN_MODE,
    CONF_FAN_MODE_COMMAND_TOPIC,
    CONF_FAN_MODE_STATE_TOPIC,
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_MODE,
    CONF_MODE_COMMAND_TOPIC,
    CONF_MODE_STATE_TOPIC,
    CONF_ON_STATE,
    CONF_PRESET,
    CONF_PRESET_COMMAND_TOPIC,
    CONF_PRESET_STATE_TOPIC,
    CONF_SWING_MODE,
    CONF_SWING_MODE_COMMAND_TOPIC,
    CONF_SWING_MODE_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC,
    CONF_TARGET_TEMPERATURE_LOW,
    CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC,
    CONF_TEMPERATURE_STEP,
    CONF_TRIGGER_ID,
    CONF_VISUAL,
    CONF_MQTT_ID,
)
from esphome.core import CORE, coroutine_with_priority

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@esphome/core"]
climate_ns = cg.esphome_ns.namespace("climate")

Climate = climate_ns.class_("Climate", cg.EntityBase)
ClimateCall = climate_ns.class_("ClimateCall")
ClimateTraits = climate_ns.class_("ClimateTraits")

ClimateMode = climate_ns.enum("ClimateMode")
CLIMATE_MODES = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
    "AUTO": ClimateMode.CLIMATE_MODE_AUTO,
}
validate_climate_mode = cv.enum(CLIMATE_MODES, upper=True)

ClimateFanMode = climate_ns.enum("ClimateFanMode")
CLIMATE_FAN_MODES = {
    "ON": ClimateFanMode.CLIMATE_FAN_ON,
    "OFF": ClimateFanMode.CLIMATE_FAN_OFF,
    "AUTO": ClimateFanMode.CLIMATE_FAN_AUTO,
    "LOW": ClimateFanMode.CLIMATE_FAN_LOW,
    "MEDIUM": ClimateFanMode.CLIMATE_FAN_MEDIUM,
    "HIGH": ClimateFanMode.CLIMATE_FAN_HIGH,
    "MIDDLE": ClimateFanMode.CLIMATE_FAN_MIDDLE,
    "FOCUS": ClimateFanMode.CLIMATE_FAN_FOCUS,
    "DIFFUSE": ClimateFanMode.CLIMATE_FAN_DIFFUSE,
    "QUIET": ClimateFanMode.CLIMATE_FAN_QUIET,
}

validate_climate_fan_mode = cv.enum(CLIMATE_FAN_MODES, upper=True)

ClimatePreset = climate_ns.enum("ClimatePreset")
CLIMATE_PRESETS = {
    "NONE": ClimatePreset.CLIMATE_PRESET_NONE,
    "ECO": ClimatePreset.CLIMATE_PRESET_ECO,
    "AWAY": ClimatePreset.CLIMATE_PRESET_AWAY,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "COMFORT": ClimatePreset.CLIMATE_PRESET_COMFORT,
    "HOME": ClimatePreset.CLIMATE_PRESET_HOME,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
    "ACTIVITY": ClimatePreset.CLIMATE_PRESET_ACTIVITY,
}

validate_climate_preset = cv.enum(CLIMATE_PRESETS, upper=True)

ClimateSwingMode = climate_ns.enum("ClimateSwingMode")
CLIMATE_SWING_MODES = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

validate_climate_swing_mode = cv.enum(CLIMATE_SWING_MODES, upper=True)

# Actions
ControlAction = climate_ns.class_("ControlAction", automation.Action)
StateTrigger = climate_ns.class_("StateTrigger", automation.Trigger.template())

CLIMATE_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(Climate),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTClimateComponent),
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                cv.Optional(CONF_MIN_TEMPERATURE): cv.temperature,
                cv.Optional(CONF_MAX_TEMPERATURE): cv.temperature,
                cv.Optional(CONF_TEMPERATURE_STEP): cv.float_with_unit(
                    "visual_temperature", "(°C|° C|°|C|° K|° K|K|°F|° F|F)?"
                ),
            }
        ),
        cv.Optional(CONF_ACTION_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_AWAY_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_AWAY_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_FAN_MODE_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_FAN_MODE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_MODE_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_MODE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_PRESET_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_PRESET_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_SWING_MODE_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_SWING_MODE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
            }
        ),
    }
)


async def setup_climate_core_(var, config):
    await setup_entity(var, config)

    visual = config[CONF_VISUAL]
    if CONF_MIN_TEMPERATURE in visual:
        cg.add(var.set_visual_min_temperature_override(visual[CONF_MIN_TEMPERATURE]))
    if CONF_MAX_TEMPERATURE in visual:
        cg.add(var.set_visual_max_temperature_override(visual[CONF_MAX_TEMPERATURE]))
    if CONF_TEMPERATURE_STEP in visual:
        cg.add(var.set_visual_temperature_step_override(visual[CONF_TEMPERATURE_STEP]))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if CONF_ACTION_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_action_state_topic(config[CONF_ACTION_STATE_TOPIC]))
        if CONF_AWAY_COMMAND_TOPIC in config:
            cg.add(mqtt_.set_custom_away_command_topic(config[CONF_AWAY_COMMAND_TOPIC]))
        if CONF_AWAY_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_away_state_topic(config[CONF_AWAY_STATE_TOPIC]))
        if CONF_CURRENT_TEMPERATURE_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_current_temperature_state_topic(
                    config[CONF_CURRENT_TEMPERATURE_STATE_TOPIC]
                )
            )
        if CONF_FAN_MODE_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_fan_mode_command_topic(
                    config[CONF_FAN_MODE_COMMAND_TOPIC]
                )
            )
        if CONF_FAN_MODE_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_fan_mode_state_topic(config[CONF_FAN_MODE_STATE_TOPIC])
            )
        if CONF_MODE_COMMAND_TOPIC in config:
            cg.add(mqtt_.set_custom_mode_command_topic(config[CONF_MODE_COMMAND_TOPIC]))
        if CONF_MODE_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_mode_state_topic(config[CONF_MODE_STATE_TOPIC]))
        if CONF_PRESET_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_preset_command_topic(config[CONF_PRESET_COMMAND_TOPIC])
            )
        if CONF_PRESET_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_preset_state_topic(config[CONF_PRESET_STATE_TOPIC]))
        if CONF_SWING_MODE_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_swing_mode_command_topic(
                    config[CONF_SWING_MODE_COMMAND_TOPIC]
                )
            )
        if CONF_SWING_MODE_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_swing_mode_state_topic(
                    config[CONF_SWING_MODE_STATE_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_command_topic(
                    config[CONF_TARGET_TEMPERATURE_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_state_topic(
                    config[CONF_TARGET_TEMPERATURE_STATE_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_high_command_topic(
                    config[CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_high_state_topic(
                    config[CONF_TARGET_TEMPERATURE_HIGH_STATE_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_low_command_topic(
                    config[CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_temperature_state_topic(
                    config[CONF_TARGET_TEMPERATURE_LOW_STATE_TOPIC]
                )
            )

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_climate(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_climate(var))
    await setup_climate_core_(var, config)


CLIMATE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Climate),
        cv.Optional(CONF_MODE): cv.templatable(validate_climate_mode),
        cv.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.templatable(cv.temperature),
        cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.templatable(cv.temperature),
        cv.Optional(CONF_AWAY): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_FAN_MODE, "fan_mode"): cv.templatable(
            validate_climate_fan_mode
        ),
        cv.Exclusive(CONF_CUSTOM_FAN_MODE, "fan_mode"): cv.templatable(
            cv.string_strict
        ),
        cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(validate_climate_preset),
        cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.templatable(cv.string_strict),
        cv.Optional(CONF_SWING_MODE): cv.templatable(validate_climate_swing_mode),
    }
)


@automation.register_action(
    "climate.control", ControlAction, CLIMATE_CONTROL_ACTION_SCHEMA
)
async def climate_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_MODE in config:
        template_ = await cg.templatable(config[CONF_MODE], args, ClimateMode)
        cg.add(var.set_mode(template_))
    if CONF_TARGET_TEMPERATURE in config:
        template_ = await cg.templatable(config[CONF_TARGET_TEMPERATURE], args, float)
        cg.add(var.set_target_temperature(template_))
    if CONF_TARGET_TEMPERATURE_LOW in config:
        template_ = await cg.templatable(
            config[CONF_TARGET_TEMPERATURE_LOW], args, float
        )
        cg.add(var.set_target_temperature_low(template_))
    if CONF_TARGET_TEMPERATURE_HIGH in config:
        template_ = await cg.templatable(
            config[CONF_TARGET_TEMPERATURE_HIGH], args, float
        )
        cg.add(var.set_target_temperature_high(template_))
    if CONF_AWAY in config:
        template_ = await cg.templatable(config[CONF_AWAY], args, bool)
        cg.add(var.set_away(template_))
    if CONF_FAN_MODE in config:
        template_ = await cg.templatable(config[CONF_FAN_MODE], args, ClimateFanMode)
        cg.add(var.set_fan_mode(template_))
    if CONF_CUSTOM_FAN_MODE in config:
        template_ = await cg.templatable(
            config[CONF_CUSTOM_FAN_MODE], args, cg.std_string
        )
        cg.add(var.set_custom_fan_mode(template_))
    if CONF_PRESET in config:
        template_ = await cg.templatable(config[CONF_PRESET], args, ClimatePreset)
        cg.add(var.set_preset(template_))
    if CONF_CUSTOM_PRESET in config:
        template_ = await cg.templatable(
            config[CONF_CUSTOM_PRESET], args, cg.std_string
        )
        cg.add(var.set_custom_preset(template_))
    if CONF_SWING_MODE in config:
        template_ = await cg.templatable(
            config[CONF_SWING_MODE], args, ClimateSwingMode
        )
        cg.add(var.set_swing_mode(template_))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_CLIMATE")
    cg.add_global(climate_ns.using)
