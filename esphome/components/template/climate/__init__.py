from esphome import automation
import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_CURRENT_TEMPERATURE,
    CONF_CUSTOM_FAN_MODE,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESET,
    CONF_CUSTOM_PRESETS,
    CONF_FAN_MODE,
    CONF_ID,
    CONF_MODE,
    CONF_OPTIMISTIC,
    CONF_PRESET,
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
CONF_SET_CUSTOM_FAN_MODE_ACTION = "set_custom_fan_mode_action"
CONF_SET_CUSTOM_PRESET_ACTION = "set_custom_preset_action"
CONF_SET_FAN_MODE_ACTION = "set_fan_mode_action"
CONF_SET_MODE_ACTION = "set_mode_action"
CONF_SET_PRESET_ACTION = "set_preset_action"
CONF_SET_SWING_MODE_ACTION = "set_swing_mode_action"
CONF_SET_TARGET_HUMIDITY_ACTION = "set_target_humidity_action"
CONF_SET_TARGET_TEMPERATURE_ACTION = "set_target_temperature_action"
CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION = "set_target_temperature_high_action"
CONF_SET_TARGET_TEMPERATURE_LOW_ACTION = "set_target_temperature_low_action"

TemplateClimate = template_ns.class_("TemplateClimate", climate.Climate, cg.Component)


def _validate_two_point(config):
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


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(TemplateClimate)
    .extend(
        {
            cv.Optional(CONF_CURRENT_TEMPERATURE): cv.returning_lambda,
            cv.Optional(CONF_CURRENT_HUMIDITY): cv.returning_lambda,
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.returning_lambda,
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW): cv.returning_lambda,
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH): cv.returning_lambda,
            cv.Optional(CONF_TARGET_HUMIDITY): cv.returning_lambda,
            cv.Optional(CONF_MODE): cv.returning_lambda,
            cv.Optional(CONF_ACTION): cv.returning_lambda,
            cv.Exclusive(CONF_FAN_MODE, "fan_mode"): cv.returning_lambda,
            cv.Exclusive(CONF_CUSTOM_FAN_MODE, "fan_mode"): cv.returning_lambda,
            cv.Optional(CONF_SWING_MODE): cv.returning_lambda,
            cv.Exclusive(CONF_PRESET, "preset"): cv.returning_lambda,
            cv.Exclusive(CONF_CUSTOM_PRESET, "preset"): cv.returning_lambda,
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
            cv.Optional(CONF_SET_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_LOW_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(
                CONF_SET_TARGET_HUMIDITY_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_SET_FAN_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_SET_CUSTOM_FAN_MODE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_SET_SWING_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_CUSTOM_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
            # optimistic: controls whether the entity state is updated immediately after a
            #   set_*_action fires (True) or only after the next read-lambda poll (False).
            #   When no read lambdas are configured the entity state is always updated
            #   immediately, since there is nothing polling the device.
            cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_two_point,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    if CONF_CURRENT_TEMPERATURE in config:
        template_ = await cg.process_lambda(
            config[CONF_CURRENT_TEMPERATURE],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_current_temperature_lambda(template_))

    if CONF_CURRENT_HUMIDITY in config:
        template_ = await cg.process_lambda(
            config[CONF_CURRENT_HUMIDITY],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_current_humidity_lambda(template_))

    if CONF_TARGET_TEMPERATURE in config:
        template_ = await cg.process_lambda(
            config[CONF_TARGET_TEMPERATURE],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_target_temperature_lambda(template_))

    if CONF_TARGET_TEMPERATURE_LOW in config:
        template_ = await cg.process_lambda(
            config[CONF_TARGET_TEMPERATURE_LOW],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_target_temperature_low_lambda(template_))

    if CONF_TARGET_TEMPERATURE_HIGH in config:
        template_ = await cg.process_lambda(
            config[CONF_TARGET_TEMPERATURE_HIGH],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_target_temperature_high_lambda(template_))

    if CONF_TARGET_HUMIDITY in config:
        template_ = await cg.process_lambda(
            config[CONF_TARGET_HUMIDITY],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_target_humidity_lambda(template_))

    if CONF_MODE in config:
        template_ = await cg.process_lambda(
            config[CONF_MODE],
            [],
            return_type=cg.optional.template(climate.ClimateMode),
        )
        cg.add(var.set_mode_lambda(template_))

    if CONF_ACTION in config:
        template_ = await cg.process_lambda(
            config[CONF_ACTION],
            [],
            return_type=cg.optional.template(climate.ClimateAction),
        )
        cg.add(var.set_action_lambda(template_))

    if CONF_FAN_MODE in config:
        template_ = await cg.process_lambda(
            config[CONF_FAN_MODE],
            [],
            return_type=cg.optional.template(climate.ClimateFanMode),
        )
        cg.add(var.set_fan_mode_lambda(template_))

    if CONF_CUSTOM_FAN_MODE in config:
        template_ = await cg.process_lambda(
            config[CONF_CUSTOM_FAN_MODE],
            [],
            return_type=cg.optional.template(cg.std_string),
        )
        cg.add(var.set_custom_fan_mode_lambda(template_))

    if CONF_SWING_MODE in config:
        template_ = await cg.process_lambda(
            config[CONF_SWING_MODE],
            [],
            return_type=cg.optional.template(climate.ClimateSwingMode),
        )
        cg.add(var.set_swing_mode_lambda(template_))

    if CONF_PRESET in config:
        template_ = await cg.process_lambda(
            config[CONF_PRESET],
            [],
            return_type=cg.optional.template(climate.ClimatePreset),
        )
        cg.add(var.set_preset_lambda(template_))

    if CONF_CUSTOM_PRESET in config:
        template_ = await cg.process_lambda(
            config[CONF_CUSTOM_PRESET],
            [],
            return_type=cg.optional.template(cg.std_string),
        )
        cg.add(var.set_custom_preset_lambda(template_))

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

    if CONF_SET_MODE_ACTION in config:
        await automation.build_automation(
            var.get_set_mode_trigger(),
            [(climate.ClimateMode, "x")],
            config[CONF_SET_MODE_ACTION],
        )

    if CONF_SET_TARGET_TEMPERATURE_ACTION in config:
        await automation.build_automation(
            var.get_set_target_temperature_trigger(),
            [(float, "x")],
            config[CONF_SET_TARGET_TEMPERATURE_ACTION],
        )

    if CONF_SET_TARGET_TEMPERATURE_LOW_ACTION in config:
        await automation.build_automation(
            var.get_set_target_temperature_low_trigger(),
            [(float, "x")],
            config[CONF_SET_TARGET_TEMPERATURE_LOW_ACTION],
        )

    if CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION in config:
        await automation.build_automation(
            var.get_set_target_temperature_high_trigger(),
            [(float, "x")],
            config[CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION],
        )

    if CONF_SET_TARGET_HUMIDITY_ACTION in config:
        await automation.build_automation(
            var.get_set_target_humidity_trigger(),
            [(float, "x")],
            config[CONF_SET_TARGET_HUMIDITY_ACTION],
        )

    if CONF_SET_FAN_MODE_ACTION in config:
        await automation.build_automation(
            var.get_set_fan_mode_trigger(),
            [(climate.ClimateFanMode, "x")],
            config[CONF_SET_FAN_MODE_ACTION],
        )

    if CONF_SET_CUSTOM_FAN_MODE_ACTION in config:
        await automation.build_automation(
            var.get_set_custom_fan_mode_trigger(),
            [(cg.std_string, "x")],
            config[CONF_SET_CUSTOM_FAN_MODE_ACTION],
        )

    if CONF_SET_SWING_MODE_ACTION in config:
        await automation.build_automation(
            var.get_set_swing_mode_trigger(),
            [(climate.ClimateSwingMode, "x")],
            config[CONF_SET_SWING_MODE_ACTION],
        )

    if CONF_SET_PRESET_ACTION in config:
        await automation.build_automation(
            var.get_set_preset_trigger(),
            [(climate.ClimatePreset, "x")],
            config[CONF_SET_PRESET_ACTION],
        )

    if CONF_SET_CUSTOM_PRESET_ACTION in config:
        await automation.build_automation(
            var.get_set_custom_preset_trigger(),
            [(cg.std_string, "x")],
            config[CONF_SET_CUSTOM_PRESET_ACTION],
        )

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
