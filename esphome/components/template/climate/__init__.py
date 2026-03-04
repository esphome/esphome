from esphome import automation
import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_ID,
    CONF_OPTIMISTIC,
    CONF_SUPPORTED_FAN_MODES,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TARGET_TEMPERATURE,
)

from .. import template_ns

CONF_SET_FAN_MODE_ACTION = "set_fan_mode_action"
CONF_SET_MODE_ACTION = "set_mode_action"
CONF_SET_PRESET_ACTION = "set_preset_action"
CONF_SET_SWING_MODE_ACTION = "set_swing_mode_action"
CONF_SET_TARGET_TEMPERATURE_ACTION = "set_target_temperature_action"

TemplateClimate = template_ns.class_("TemplateClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = (
    climate.climate_schema(TemplateClimate)
    .extend(
        {
            cv.Optional(CONF_CURRENT_TEMPERATURE): cv.returning_lambda,
            cv.Optional(CONF_TARGET_TEMPERATURE): cv.returning_lambda,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                climate.validate_climate_mode
            ),
            cv.Optional(CONF_SUPPORTED_FAN_MODES): cv.ensure_list(
                climate.validate_climate_fan_mode
            ),
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                climate.validate_climate_swing_mode
            ),
            cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(
                climate.validate_climate_preset
            ),
            cv.Optional(CONF_SET_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(
                CONF_SET_TARGET_TEMPERATURE_ACTION
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_SET_FAN_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_SWING_MODE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_SET_PRESET_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_OPTIMISTIC, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
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

    if CONF_TARGET_TEMPERATURE in config:
        template_ = await cg.process_lambda(
            config[CONF_TARGET_TEMPERATURE],
            [],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_target_temperature_lambda(template_))

    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))

    if CONF_SUPPORTED_FAN_MODES in config:
        cg.add(var.set_supported_fan_modes(config[CONF_SUPPORTED_FAN_MODES]))

    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))

    if CONF_SUPPORTED_PRESETS in config:
        cg.add(var.set_supported_presets(config[CONF_SUPPORTED_PRESETS]))

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

    if CONF_SET_FAN_MODE_ACTION in config:
        await automation.build_automation(
            var.get_set_fan_mode_trigger(),
            [(climate.ClimateFanMode, "x")],
            config[CONF_SET_FAN_MODE_ACTION],
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

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
