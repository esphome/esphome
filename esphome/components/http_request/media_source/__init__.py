import esphome.codegen as cg
from esphome.components import media_source
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["http_request", "media_source", "audio"]

HTTPMediaSource = http_request_ns.class_(
    "HTTPMediaSource", cg.Component, media_source.MediaSource
)

CONF_BUFFER_SIZE = "buffer_size"
CONF_TASK_STACK_IN_PSRAM = "task_stack_in_psram"

CONFIG_SCHEMA = (
    media_source.media_source_schema(
        HTTPMediaSource,
        media_player=False,
    )
    .extend(
        {
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Optional(CONF_BUFFER_SIZE, default=51200): cv.int_range(
                min=8 * 1024, max=500 * 1024
            ),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    if CONF_BUFFER_SIZE in config:
        cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))

    # Set URI prefix to handle both http and https
    cg.add(var.set_uri_prefix("http"))
