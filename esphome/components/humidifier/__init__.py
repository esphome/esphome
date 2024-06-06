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
    CONF_CURRENT_HUMIDITY_STATE_TOPIC,
    CONF_ID,
    CONF_MAX_HUMIDITY,
    CONF_MIN_HUMIDITY,
    CONF_MODE,
    CONF_MODE_COMMAND_TOPIC,
    CONF_MODE_STATE_TOPIC,
    CONF_ON_STATE,
    CONF_PRESET,
    CONF_TARGET_HUMIDITY,
    CONF_TARGET_HUMIDITY_COMMAND_TOPIC,
    CONF_TARGET_HUMIDITY_STATE_TOPIC,
    CONF_TARGET_HUMIDITY_HIGH,
    CONF_TARGET_HUMIDITY_HIGH_COMMAND_TOPIC,
    CONF_TARGET_HUMIDITY_HIGH_STATE_TOPIC,
    CONF_TARGET_HUMIDITY_LOW,
    CONF_TARGET_HUMIDITY_LOW_COMMAND_TOPIC,
    CONF_TARGET_HUMIDITY_LOW_STATE_TOPIC,
    CONF_HUMIDITY_STEP,
    CONF_TRIGGER_ID,
    CONF_VISUAL,
    CONF_MQTT_ID,
)
from esphome.core import CORE, coroutine_with_priority

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@Jaco1990"]
humidifier_ns = cg.esphome_ns.namespace("humidifier")

Humidifier = humidifier_ns.class_("Humidifier", cg.EntityBase)
HumidifierCall = humidifier_ns.class_("HumidifierCall")
HumidifierTraits = humidifier_ns.class_("HumidifierTraits")

HumidifierMode = humidifier_ns.enum("HumidifierMode")
HUMIDIFIER_MODES = {
    "OFF": HumidifierMode.HUMIDIFIER_MODE_OFF,
    "HUMIDIFY_DEHUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY,
    "HUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_HUMIDIFY,
    "DEHUMIDIFY": HumidifierMode.HUMIDIFIER_MODE_DEHUMIDIFY,
    "AUTO": HumidifierMode.HUMIDIFIER_MODE_AUTO,
}
validate_humidifier_mode = cv.enum(HUMIDIFIER_MODES, upper=True)

HumidifierPreset = humidifier_ns.enum("HumidifierPreset")
HUMIDIFIER_PRESETS = {
    "NORMAL": HumidifierPreset.HUMIDIFIER_PRESET_NORMAL,
    "ECO": HumidifierPreset.HUMIDIFIER_PRESET_ECO,
    "AWAY": HumidifierPreset.HUMIDIFIER_PRESET_AWAY,
    "BOOST": HumidifierPreset.HUMIDIFIER_PRESET_BOOST,
    "COMFORT": HumidifierPreset.HUMIDIFIER_PRESET_COMFORT,
    "HOME": HumidifierPreset.HUMIDIFIER_PRESET_HOME,
    "SLEEP": HumidifierPreset.HUMIDIFIER_PRESET_SLEEP,
    "ACTIVITY": HumidifierPreset.HUMIDIFIER_PRESET_ACTIVITY,
}

validate_humidifier_preset = cv.enum(HUMIDIFIER_PRESETS, upper=True)

# Actions
ControlAction = humidifier_ns.class_("ControlAction", automation.Action)
StateTrigger = humidifier_ns.class_("StateTrigger", automation.Trigger.template())

HUMIDIFIER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    cv.MQTT_COMMAND_COMPONENT_SCHEMA
).extend(
    {
        cv.GenerateID(): cv.declare_id(Humidifier),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTHumidifierComponent),
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                cv.Optional(CONF_MIN_HUMIDITY): cv.humidity,
                cv.Optional(CONF_MAX_HUMIDITY): cv.humidity,
                cv.Optional(CONF_HUMIDITY_STEP): cv.humidity,
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
        cv.Optional(CONF_CURRENT_HUMIDITY_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_MODE_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_MODE_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_HIGH_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_HIGH_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_LOW_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_LOW_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
            }
        ),
    }
)


async def setup_humidifier_core_(var, config):
    await setup_entity(var, config)

    visual = config[CONF_VISUAL]
    if CONF_MIN_HUMIDITY in visual:
        cg.add(var.set_visual_min_humidity_override(visual[CONF_MIN_HUMIDITY]))
    if CONF_MAX_HUMIDITY in visual:
        cg.add(var.set_visual_max_humidity_override(visual[CONF_MAX_HUMIDITY]))
    if CONF_HUMIDITY_STEP in visual:
        cg.add(var.set_visual_humidity_step_override(visual[CONF_HUMIDITY_STEP]))

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if CONF_ACTION_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_action_state_topic(config[CONF_ACTION_STATE_TOPIC]))
        if CONF_AWAY_COMMAND_TOPIC in config:
            cg.add(mqtt_.set_custom_away_command_topic(config[CONF_AWAY_COMMAND_TOPIC]))
        if CONF_AWAY_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_away_state_topic(config[CONF_AWAY_STATE_TOPIC]))
        if CONF_CURRENT_HUMIDITY_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_current_humidity_state_topic(
                    config[CONF_CURRENT_HUMIDITY_STATE_TOPIC]
                )
            )
        if CONF_MODE_COMMAND_TOPIC in config:
            cg.add(mqtt_.set_custom_mode_command_topic(config[CONF_MODE_COMMAND_TOPIC]))
        if CONF_MODE_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_mode_state_topic(config[CONF_MODE_STATE_TOPIC]))

        if CONF_TARGET_HUMIDITY_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_command_topic(
                    config[CONF_TARGET_HUMIDITY_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_HUMIDITY_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_state_topic(
                    config[CONF_TARGET_HUMIDITY_STATE_TOPIC]
                )
            )
        if CONF_TARGET_HUMIDITY_HIGH_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_high_command_topic(
                    config[CONF_TARGET_HUMIDITY_HIGH_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_HUMIDITY_HIGH_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_high_state_topic(
                    config[CONF_TARGET_HUMIDITY_HIGH_STATE_TOPIC]
                )
            )
        if CONF_TARGET_HUMIDITY_LOW_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_low_command_topic(
                    config[CONF_TARGET_HUMIDITY_LOW_COMMAND_TOPIC]
                )
            )
        if CONF_TARGET_HUMIDITY_LOW_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_target_humidity_state_topic(
                    config[CONF_TARGET_HUMIDITY_LOW_STATE_TOPIC]
                )
            )

    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_humidifier(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_humidifier(var))
    await setup_humidifier_core_(var, config)


HUMIDIFIER_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Humidifier),
        cv.Optional(CONF_MODE): cv.templatable(validate_humidifier_mode),
        cv.Optional(CONF_TARGET_HUMIDITY): cv.templatable(cv.humidity),
        cv.Optional(CONF_TARGET_HUMIDITY_LOW): cv.templatable(cv.humidity),
        cv.Optional(CONF_TARGET_HUMIDITY_HIGH): cv.templatable(cv.humidity),
        cv.Optional(CONF_AWAY): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(validate_humidifier_preset),
    }
)


@automation.register_action(
    "humidifier.control", ControlAction, HUMIDIFIER_CONTROL_ACTION_SCHEMA
)
async def humidifier_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_MODE in config:
        template_ = await cg.templatable(config[CONF_MODE], args, HumidifierMode)
        cg.add(var.set_mode(template_))
    if CONF_TARGET_HUMIDITY in config:
        template_ = await cg.templatable(config[CONF_TARGET_HUMIDITY], args, float)
        cg.add(var.set_target_humidity(template_))
    if CONF_TARGET_HUMIDITY_LOW in config:
        template_ = await cg.templatable(config[CONF_TARGET_HUMIDITY_LOW], args, float)
        cg.add(var.set_target_humidity_low(template_))
    if CONF_TARGET_HUMIDITY_HIGH in config:
        template_ = await cg.templatable(config[CONF_TARGET_HUMIDITY_HIGH], args, float)
        cg.add(var.set_target_humidity_high(template_))
    if CONF_AWAY in config:
        template_ = await cg.templatable(config[CONF_AWAY], args, bool)
        cg.add(var.set_away(template_))
    if CONF_PRESET in config:
        template_ = await cg.templatable(config[CONF_PRESET], args, HumidifierPreset)
        cg.add(var.set_preset(template_))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_HUMIDIFIER")
    cg.add_global(humidifier_ns.using)
