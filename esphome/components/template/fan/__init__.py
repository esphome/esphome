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
            cv.Optional(CONF_SPEED_COUNT): fan.validate_speed_count,
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

    cg.add_optional(var.set_speed_count, config.get(CONF_SPEED_COUNT))

    fan.set_preset_modes(var, config.get(CONF_PRESET_MODES, []))
