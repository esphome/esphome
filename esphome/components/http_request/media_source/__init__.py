import esphome.codegen as cg
from esphome.components import media_source, network, psram
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TASK_STACK_IN_PSRAM
from esphome.types import ConfigType

from .. import CONF_HTTP_REQUEST_ID, HttpRequestComponent, http_request_ns

CODEOWNERS = ["@kahrendt"]
AUTO_LOAD = ["audio"]
DEPENDENCIES = ["http_request"]

HTTPRequestMediaSource = http_request_ns.class_(
    "HTTPRequestMediaSource", cg.Component, media_source.MediaSource
)


def _register_network_requirements(config: ConfigType) -> ConfigType:
    """Register network requirements for http_request media_source component."""
    from esphome.components import socket

    # http_request media_source uses one socket (single-pipeline)
    socket.consume_sockets(1, "http_request_media_source")(config)

    network.require_high_performance_networking()

    return config


CONFIG_SCHEMA = cv.All(
    media_source.media_source_schema(
        HTTPRequestMediaSource,
    )
    .extend(
        {
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Optional(CONF_BUFFER_SIZE, default=51200): cv.int_range(
                min=8 * 1024, max=500 * 1024
            ),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.All(
                cv.boolean, cv.requires_component(psram.DOMAIN)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _register_network_requirements,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)
    await cg.register_parented(var, config[CONF_HTTP_REQUEST_ID])

    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
