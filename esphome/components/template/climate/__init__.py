from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_CURRENT_TEMPERATURE,
    CONF_CUSTOM_FAN_MODE,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESET,
    CONF_CUSTOM_PRESETS,
    CONF_FAN_MODE,
    CONF_HUMIDITY_SENSOR,
    CONF_ID,
    CONF_MODE,
    CONF_OPTIMISTIC,
    CONF_PRESET,
    CONF_SENSOR,
    CONF_SUPPORTED_FAN_MODES,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_SWING_MODE,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_LOW,
)

from .. import template_ns

CONF_CURRENT_HUMIDITY = "current_humidity"
CONF_TARGET_HUMIDITY = "target_humidity"
CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE = "supports_two_point_target_temperature"
CONF_SUPPORTS_TARGET_HUMIDITY = "supports_target_humidity"
CONF_SUPPORTS_ACTION = "supports_action"

TemplateClimate = template_ns.class_("TemplateClimate", climate.Climate, cg.Component)
TemplateClimatePublishAction = template_ns.class_(
    "TemplateClimatePublishAction",
    automation.Action,
    cg.Parented.template(TemplateClimate),
)

CONFIG_SCHEMA = (
    climate.climate_schema(TemplateClimate)
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_SUPPORTS_ACTION, default=False): cv.boolean,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                climate.validate_climate_mode
            ),
            cv.Optional(CONF_SUPPORTED_FAN_MODES): cv.ensure_list(
                climate.validate_climate_fan_mode
            ),
            cv.Optional(CONF_CUSTOM_FAN_MODES): cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                climate.validate_climate_swing_mode
            ),
            cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(
                climate.validate_climate_preset
            ),
            cv.Optional(CONF_CUSTOM_PRESETS): cv.ensure_list(cv.string_strict),
            cv.Optional(
                CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE, default=False
            ): cv.boolean,
            cv.Optional(CONF_SUPPORTS_TARGET_HUMIDITY, default=False): cv.boolean,
            cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    if (sens := config.get(CONF_SENSOR)) is not None:
        cg.add(var.set_sensor(await cg.get_variable(sens)))

    if (sens := config.get(CONF_HUMIDITY_SENSOR)) is not None:
        cg.add(var.set_humidity_sensor(await cg.get_variable(sens)))

    if config[CONF_SUPPORTS_ACTION]:
        cg.add(var.set_supports_action())

    for mode in config.get(CONF_SUPPORTED_MODES, []):
        cg.add(var.add_supported_mode(mode))

    for mode in config.get(CONF_SUPPORTED_FAN_MODES, []):
        cg.add(var.add_supported_fan_mode(mode))

    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(
            var.set_supported_custom_fan_modes(
                cg.ArrayInitializer(*config[CONF_CUSTOM_FAN_MODES])
            )
        )

    for mode in config.get(CONF_SUPPORTED_SWING_MODES, []):
        cg.add(var.add_supported_swing_mode(mode))

    for preset in config.get(CONF_SUPPORTED_PRESETS, []):
        cg.add(var.add_supported_preset(preset))

    if CONF_CUSTOM_PRESETS in config:
        cg.add(
            var.set_supported_custom_presets(
                cg.ArrayInitializer(*config[CONF_CUSTOM_PRESETS])
            )
        )

    if config[CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE]:
        cg.add(var.set_supports_two_point_target_temperature())

    if config[CONF_SUPPORTS_TARGET_HUMIDITY]:
        cg.add(var.set_supports_target_humidity())

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))


def _validate_publish_two_point(config):
    has_low = CONF_TARGET_TEMPERATURE_LOW in config
    has_high = CONF_TARGET_TEMPERATURE_HIGH in config
    if has_low != has_high:
        raise cv.Invalid(
            f"'{CONF_TARGET_TEMPERATURE_LOW}' and '{CONF_TARGET_TEMPERATURE_HIGH}' must be used together"
        )
    if (has_low or has_high) and CONF_TARGET_TEMPERATURE in config:
        raise cv.Invalid(
            f"'{CONF_TARGET_TEMPERATURE}' cannot be used together with "
            f"'{CONF_TARGET_TEMPERATURE_LOW}'/'{CONF_TARGET_TEMPERATURE_HIGH}'"
        )
    return config


CLIMATE_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TemplateClimate),
            cv.Optional(CONF_CURRENT_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_CURRENT_HUMIDITY): cv.templatable(cv.percentage_int),
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.templatable(cv.temperature),
            cv.Optional(CONF_TARGET_HUMIDITY): cv.templatable(cv.percentage_int),
            cv.Optional(CONF_MODE): cv.templatable(climate.validate_climate_mode),
            cv.Optional(CONF_ACTION): cv.templatable(climate.validate_climate_action),
            cv.Exclusive(CONF_FAN_MODE, "fan_mode"): cv.templatable(
                climate.validate_climate_fan_mode
            ),
            cv.Exclusive(CONF_CUSTOM_FAN_MODE, "fan_mode"): cv.templatable(
                cv.string_strict
            ),
            cv.Optional(CONF_SWING_MODE): cv.templatable(
                climate.validate_climate_swing_mode
            ),
            cv.Exclusive(CONF_PRESET, "preset"): cv.templatable(
                climate.validate_climate_preset
            ),
            cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.templatable(
                cv.string_strict
            ),
        }
    ),
    cv.has_at_least_one_key(
        CONF_CURRENT_TEMPERATURE,
        CONF_CURRENT_HUMIDITY,
        CONF_TARGET_TEMPERATURE,
        CONF_TARGET_TEMPERATURE_LOW,
        CONF_TARGET_TEMPERATURE_HIGH,
        CONF_TARGET_HUMIDITY,
        CONF_MODE,
        CONF_ACTION,
        CONF_FAN_MODE,
        CONF_CUSTOM_FAN_MODE,
        CONF_SWING_MODE,
        CONF_PRESET,
        CONF_CUSTOM_PRESET,
    ),
    _validate_publish_two_point,
)


@automation.register_action(
    "climate.template.publish",
    TemplateClimatePublishAction,
    CLIMATE_TEMPLATE_PUBLISH_ACTION_SCHEMA,
    synchronous=True,
)
async def climate_template_publish_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    if (v := config.get(CONF_CURRENT_TEMPERATURE)) is not None:
        cg.add(var.set_current_temperature(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_CURRENT_HUMIDITY)) is not None:
        cg.add(var.set_current_humidity(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE)) is not None:
        cg.add(var.set_target_temperature(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE_LOW)) is not None:
        cg.add(var.set_target_temperature_low(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_TARGET_TEMPERATURE_HIGH)) is not None:
        cg.add(
            var.set_target_temperature_high(await cg.templatable(v, args, cg.float_))
        )
    if (v := config.get(CONF_TARGET_HUMIDITY)) is not None:
        cg.add(var.set_target_humidity(await cg.templatable(v, args, cg.float_)))
    if (v := config.get(CONF_MODE)) is not None:
        cg.add(var.set_mode(await cg.templatable(v, args, climate.ClimateMode)))
    if (v := config.get(CONF_ACTION)) is not None:
        cg.add(var.set_action(await cg.templatable(v, args, climate.ClimateAction)))
    if (v := config.get(CONF_FAN_MODE)) is not None:
        cg.add(var.set_fan_mode(await cg.templatable(v, args, climate.ClimateFanMode)))
    if (v := config.get(CONF_CUSTOM_FAN_MODE)) is not None:
        cg.add(var.set_custom_fan_mode(await cg.templatable(v, args, cg.std_string)))
    if (v := config.get(CONF_SWING_MODE)) is not None:
        cg.add(
            var.set_swing_mode(await cg.templatable(v, args, climate.ClimateSwingMode))
        )
    if (v := config.get(CONF_PRESET)) is not None:
        cg.add(var.set_preset(await cg.templatable(v, args, climate.ClimatePreset)))
    if (v := config.get(CONF_CUSTOM_PRESET)) is not None:
        cg.add(var.set_custom_preset(await cg.templatable(v, args, cg.std_string)))

    return var
