import esphome.codegen as cg
from esphome.components import fan
from esphome.components.fan import validate_preset_modes
import esphome.config_validation as cv
from esphome.const import CONF_PRESET_MODES, CONF_SPEED_COUNT

from .. import template_ns

CODEOWNERS = ["@ssieb"]

TemplateFan = template_ns.class_("TemplateFan", cg.Component, fan.Fan)

CONF_HAS_DIRECTION = "has_direction"
CONF_HAS_OSCILLATING = "has_oscillating"

CONFIG_SCHEMA = (
    fan.fan_schema(TemplateFan)
    .extend(
        {
            cv.Optional(CONF_HAS_DIRECTION, default=False): cv.boolean,
            cv.Optional(CONF_HAS_OSCILLATING, default=False): cv.boolean,
            cv.Optional(CONF_SPEED_COUNT): cv.int_range(min=1),
            cv.Optional(CONF_PRESET_MODES): validate_preset_modes,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)

    cg.add(var.set_has_direction(config[CONF_HAS_DIRECTION]))
    cg.add(var.set_has_oscillating(config[CONF_HAS_OSCILLATING]))

    if CONF_SPEED_COUNT in config:
        cg.add(var.set_speed_count(config[CONF_SPEED_COUNT]))

    if CONF_PRESET_MODES in config:
        cg.add(var.set_preset_modes(config[CONF_PRESET_MODES]))
