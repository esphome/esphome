"""Sendspin media source component for ESPHome."""

import esphome.codegen as cg
from esphome.components import media_source
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["media_source", "audio"]

CONF_TASK_STACK_IN_PSRAM = "task_stack_in_psram"

SendspinMediaSource = sendspin_ns.class_(
    "SendspinMediaSource",
    media_source.MediaSource,
    cg.Component,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SendspinMediaSource),
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
        cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.boolean,
        cv.Optional(CONF_BUFFER_SIZE, default=1000000): cv.int_range(min=25000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for color noise media source."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sendspin_hub = await cg.get_variable(config[CONF_SENDSPIN_ID])
    await cg.register_parented(var, sendspin_hub)

    cg.add(sendspin_hub.set_buffer_size(config[CONF_BUFFER_SIZE]))

    cg.add_define("USE_SENDSPIN_PLAYER")

    cg.add(var.set_uri_prefix("sendspin"))

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
