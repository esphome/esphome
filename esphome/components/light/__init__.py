import esphome.automation as auto
import esphome.codegen as cg
from esphome.components import mqtt, power_supply, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_CORRECT,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_EFFECTS,
    CONF_FLASH_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_POWER_SUPPLY,
    CONF_RESTORE_MODE,
    CONF_TRIGGER_ID,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
    CONF_WEB_SERVER_ID,
)
from esphome.core import coroutine_with_priority
from esphome.cpp_helpers import setup_entity

from .automation import light_control_to_code  # noqa
from .effects import (
    ADDRESSABLE_EFFECTS,
    BINARY_EFFECTS,
    EFFECTS_REGISTRY,
    MONOCHROMATIC_EFFECTS,
    RGB_EFFECTS,
    validate_effects,
)
from .types import (  # noqa
    AddressableLight,
    AddressableLightState,
    LightOutput,
    LightState,
    LightStateTrigger,
    LightTurnOffTrigger,
    LightTurnOnTrigger,
    light_ns,
)

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True

LightRestoreMode = light_ns.enum("LightRestoreMode")
RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": LightRestoreMode.LIGHT_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": LightRestoreMode.LIGHT_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": LightRestoreMode.LIGHT_ALWAYS_OFF,
    "ALWAYS_ON": LightRestoreMode.LIGHT_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": LightRestoreMode.LIGHT_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": LightRestoreMode.LIGHT_RESTORE_INVERTED_DEFAULT_ON,
    "RESTORE_AND_OFF": LightRestoreMode.LIGHT_RESTORE_AND_OFF,
    "RESTORE_AND_ON": LightRestoreMode.LIGHT_RESTORE_AND_ON,
}

LIGHT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(LightState),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTJSONLightComponent
            ),
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
                RESTORE_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_ON_TURN_ON): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOnTrigger),
                }
            ),
            cv.Optional(CONF_ON_TURN_OFF): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightTurnOffTrigger),
                }
            ),
            cv.Optional(CONF_ON_STATE): auto.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LightStateTrigger),
                }
            ),
        }
    )
)

BINARY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_EFFECTS): validate_effects(BINARY_EFFECTS),
    }
)

BRIGHTNESS_ONLY_LIGHT_SCHEMA = LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_GAMMA_CORRECT, default=2.8): cv.positive_float,
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_FLASH_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_EFFECTS): validate_effects(MONOCHROMATIC_EFFECTS),
    }
)

RGB_LIGHT_SCHEMA = BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.Optional(CONF_EFFECTS): validate_effects(RGB_EFFECTS),
    }
)

ADDRESSABLE_LIGHT_SCHEMA = RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AddressableLightState),
        cv.Optional(CONF_EFFECTS): validate_effects(ADDRESSABLE_EFFECTS),
        cv.Optional(CONF_COLOR_CORRECT): cv.All(
            [cv.percentage], cv.Length(min=3, max=4)
        ),
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply),
    }
)


def validate_color_temperature_channels(value):
    if (
        CONF_COLD_WHITE_COLOR_TEMPERATURE in value
        and CONF_WARM_WHITE_COLOR_TEMPERATURE in value
        and value[CONF_COLD_WHITE_COLOR_TEMPERATURE]
        >= value[CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ):
        raise cv.Invalid(
            "Color temperature of the cold white channel must be colder than that of the warm white channel.",
            path=[CONF_COLD_WHITE_COLOR_TEMPERATURE],
        )
    return value


async def setup_light_core_(light_var, output_var, config):
    await setup_entity(light_var, config)

    cg.add(light_var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if (
        default_transition_length := config.get(CONF_DEFAULT_TRANSITION_LENGTH)
    ) is not None:
        cg.add(light_var.set_default_transition_length(default_transition_length))
    if (
        flash_transition_length := config.get(CONF_FLASH_TRANSITION_LENGTH)
    ) is not None:
        cg.add(light_var.set_flash_transition_length(flash_transition_length))
    if (gamma_correct := config.get(CONF_GAMMA_CORRECT)) is not None:
        cg.add(light_var.set_gamma_correct(gamma_correct))
    effects = await cg.build_registry_list(
        EFFECTS_REGISTRY, config.get(CONF_EFFECTS, [])
    )
    cg.add(light_var.add_effects(effects))

    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], light_var)
        await auto.build_automation(trigger, [], conf)

    if (color_correct := config.get(CONF_COLOR_CORRECT)) is not None:
        cg.add(output_var.set_correction(*color_correct))

    if (power_supply_id := config.get(CONF_POWER_SUPPLY)) is not None:
        var_ = await cg.get_variable(power_supply_id)
        cg.add(output_var.set_power_supply(var_))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, light_var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, light_var, config)


async def register_light(output_var, config):
    light_var = cg.new_Pvariable(config[CONF_ID], output_var)
    cg.add(cg.App.register_light(light_var))
    await cg.register_component(light_var, config)
    await setup_light_core_(light_var, output_var, config)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_LIGHT")
    cg.add_global(light_ns.using)
