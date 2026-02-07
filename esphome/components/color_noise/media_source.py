"""Color noise media source component for ESPHome."""

import esphome.codegen as cg
from esphome.components import media_source
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SAMPLE_RATE, CONF_TASK_STACK_IN_PSRAM

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["media_source", "audio"]

color_noise_ns = cg.esphome_ns.namespace("color_noise")
ColorNoiseMediaSource = color_noise_ns.class_(
    "ColorNoiseMediaSource", cg.Component, media_source.MediaSource
)

CONF_DEFAULT_SEED = "default_seed"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ColorNoiseMediaSource),
        cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(min=8000, max=48000),
        cv.Optional(CONF_DEFAULT_SEED): cv.uint32_t,
        cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for color noise media source."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_uri_prefix("color-noise"))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))

    if CONF_DEFAULT_SEED in config:
        cg.add(var.set_default_seed(config[CONF_DEFAULT_SEED]))

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
