import esphome.codegen as cg
from esphome.components import fan, output
from esphome.components.fan import validate_preset_modes
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION_OUTPUT,
    CONF_OSCILLATION_OUTPUT,
    CONF_OUTPUT,
    CONF_PRESET_MODES,
    CONF_SPEED,
    CONF_SPEED_COUNT,
)

from .. import speed_ns

SpeedFan = speed_ns.class_("SpeedFan", cg.Component, fan.Fan)

CONFIG_SCHEMA = (
    fan.fan_schema(SpeedFan)
    .extend(
        {
            cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Optional(CONF_OSCILLATION_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_DIRECTION_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_SPEED): cv.invalid(
                "Configuring individual speeds is deprecated."
            ),
            cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1),
            cv.Optional(CONF_PRESET_MODES): validate_preset_modes,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config, config[CONF_SPEED_COUNT])
    await cg.register_component(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = await cg.get_variable(config[CONF_OSCILLATION_OUTPUT])
        cg.add(var.set_oscillating(oscillation_output))

    if CONF_DIRECTION_OUTPUT in config:
        direction_output = await cg.get_variable(config[CONF_DIRECTION_OUTPUT])
        cg.add(var.set_direction(direction_output))

    if CONF_PRESET_MODES in config:
        cg.add(var.set_preset_modes(config[CONF_PRESET_MODES]))
