import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_helpers import setup_entity
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ACTION_STATE_TOPIC,
    CONF_CURRENT_HUMIDITY_STATE_TOPIC,
    CONF_CUSTOM_PRESET,
    CONF_ID,
    CONF_MAX_HUMIDITY,
    CONF_MIN_HUMIDITY,
    CONF_MODE,
    CONF_MODE_COMMAND_TOPIC,
    CONF_MODE_STATE_TOPIC,
    CONF_ON_CONTROL,
    CONF_ON_STATE,
    CONF_PRESET,
    CONF_PRESET_COMMAND_TOPIC,
    CONF_PRESET_STATE_TOPIC,
    CONF_TARGET_HUMIDITY,
    CONF_TARGET_HUMIDITY_COMMAND_TOPIC,
    CONF_TARGET_HUMIDITY_STATE_TOPIC,
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
    "LEVEL_1": HumidifierMode.HUMIDIFIER_MODE_LEVEL_1,
    "LEVEL_2": HumidifierMode.HUMIDIFIER_MODE_LEVEL_2,
    "LEVEL_3": HumidifierMode.HUMIDIFIER_MODE_LEVEL_3,
    "PRESET": HumidifierMode.HUMIDIFIER_MODE_PRESET,
}

validate_humidifier_mode = cv.enum(HUMIDIFIER_MODES, upper=True)

HumidifierPreset = humidifier_ns.enum("HumidifierPreset")
HUMIDIFIER_PRESETS = {
    "NONE": HumidifierPreset.HUMIDIFIER_PRESET_NONE,
    "CONSTANT HUMIDITY":  HumidifierPreset.HUMIDIFIER_PRESET_CONSTANT_HUMIDITY,
    "BABY": HumidifierPreset.HUMIDIFIER_PRESET_BABY,
}

validate_humidifier_preset = cv.enum(HUMIDIFIER_PRESETS, upper=True)

CONF_CURRENT_HUMIDITY = "current_humidity"
# CONF_MIN_HUMIDITY = "min_humidity"
# CONF_MAX_HUMIDITY = "max_humidity"
# CONF_TARGET_HUMIDITY = "target_humidity"

visual_humidity = cv.float_with_unit(
    "visual_humidity", "(%)?"
)


def single_visual_humidity(value):
    if isinstance(value, dict):
        return value

    value = visual_humidity(value)
    return VISUAL_HUMIDITY_STEP_SCHEMA(
        {
            CONF_TARGET_HUMIDITY: value,
            CONF_CURRENT_HUMIDITY: value,
        }
    )


# Actions
ControlAction = humidifier_ns.class_("ControlAction", automation.Action)
StateTrigger = humidifier_ns.class_(
    "StateTrigger", automation.Trigger.template(Humidifier.operator("ref"))
)
ControlTrigger = humidifier_ns.class_(
    "ControlTrigger", automation.Trigger.template(HumidifierCall.operator("ref"))
)

VISUAL_HUMIDITY_STEP_SCHEMA = cv.Any(
    single_visual_humidity,
    cv.Schema(
        {
            cv.Required(CONF_TARGET_HUMIDITY): visual_humidity,
            cv.Required(CONF_CURRENT_HUMIDITY): visual_humidity,
        }
    ),
)

HUMIDIFIER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(Humidifier),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTHumidifierComponent),
        cv.Optional(CONF_VISUAL, default={}): cv.Schema(
            {
                # cv.Optional(CONF_MIN_HUMIDITY): cv.humidity,
                # cv.Optional(CONF_MAX_HUMIDITY): cv.humidity,
                cv.Optional(CONF_HUMIDITY_STEP): VISUAL_HUMIDITY_STEP_SCHEMA,
            }
        ),
        cv.Optional(CONF_ACTION_STATE_TOPIC): cv.All(
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
        cv.Optional(CONF_PRESET_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_PRESET_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        
        cv.Optional(CONF_TARGET_HUMIDITY_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_TARGET_HUMIDITY_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.publish_topic
        ),
        cv.Optional(CONF_ON_CONTROL): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ControlTrigger),
            }
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
        cg.add(
            var.set_visual_humidity_step_override(
                visual[CONF_HUMIDITY_STEP][CONF_TARGET_HUMIDITY],
                visual[CONF_HUMIDITY_STEP][CONF_CURRENT_HUMIDITY],
            )
        )


    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if CONF_ACTION_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_action_state_topic(config[CONF_ACTION_STATE_TOPIC]))
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
        if CONF_PRESET_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_preset_command_topic(config[CONF_PRESET_COMMAND_TOPIC])
            )
        if CONF_PRESET_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_preset_state_topic(config[CONF_PRESET_STATE_TOPIC]))
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
        
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(Humidifier.operator("ref"), "x")], conf
        )

    for conf in config.get(CONF_ON_CONTROL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(HumidifierCall.operator("ref"), "x")], conf
        )


async def register_humidifier(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_humidifier(var))
    await setup_humidifier_core_(var, config)


HUMIDIFIER_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Humidifier),
        cv.Optional(CONF_MODE): cv.templatable(validate_humidifier_mode),
        cv.Optional(CONF_TARGET_HUMIDITY): cv.templatable(cv.percentage_int),
        cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(validate_humidifier_preset),
        cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.templatable(cv.string_strict),
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
        cg.add(var.set_target_temperature(template_))
    if CONF_PRESET in config:
        template_ = await cg.templatable(config[CONF_PRESET], args, HumidifierPreset)
        cg.add(var.set_preset(template_))
    if CONF_CUSTOM_PRESET in config:
        template_ = await cg.templatable(
            config[CONF_CUSTOM_PRESET], args, cg.std_string
        )
        cg.add(var.set_custom_preset(template_))        
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_HUMIDIFIER")
    cg.add_global(humidifier_ns.using)