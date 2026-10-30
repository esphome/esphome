"""Stream one microphone source as raw PCM over the shared web server."""

import re

import esphome.codegen as cg
from esphome.components import microphone, psram, web_server, web_server_base
from esphome.components.const import CONF_IGNORE_NOT_FOUND
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_DISABLED, CONF_ID, CONF_SOURCE, CONF_WEB_SERVER_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mbrudka"]
AUTO_LOAD = ["ring_buffer"]
DEPENDENCIES = ["microphone", "web_server"]
DOMAIN = "microphone_http_stream"

CONF_STREAM_ID = "stream_id"
CONF_MAX_CLIENTS = "max_clients"
# Keep in sync with STREAM_PATH_PREFIX_LEN in microphone_http_stream.h
STREAM_PATH_PREFIX = "/audio/microphones/"
_STREAM_ID_RE = re.compile(r"[a-z][a-z0-9_-]{0,31}\Z")

microphone_http_stream_ns = cg.esphome_ns.namespace(DOMAIN)
MicrophoneHttpStream = microphone_http_stream_ns.class_(
    "MicrophoneHttpStream", cg.Component
)


def validate_stream_id(value: str) -> str:
    value = cv.string_strict(value)
    if not _STREAM_ID_RE.fullmatch(value):
        raise cv.Invalid(
            "stream_id must be 1-32 lowercase ASCII letters, digits, _ or -, starting with a letter"
        )
    return value


def _register_sockets(config: ConfigType) -> ConfigType:
    from esphome.components import socket

    socket.consume_sockets(config[CONF_MAX_CLIENTS], DOMAIN)(config)
    # ESP-IDF's HTTP server reserves three internal sockets. web_server_base
    # counts its TCP listener; account for the two UDP control sockets here.
    socket.consume_sockets(2, f"{DOMAIN}_httpd_control", socket.SocketType.UDP)(config)
    return config


def _validate_psram(config: ConfigType) -> ConfigType:
    if config[CONF_MAX_CLIENTS] > 1:
        # Same test as psram.is_guaranteed(), which is only set after psram's own final validation
        psram_config = fv.full_config.get().get(psram.DOMAIN)
        if (
            psram_config is None
            or psram_config.get(CONF_DISABLED, False)
            or psram_config.get(CONF_IGNORE_NOT_FOUND, True)
        ):
            raise cv.Invalid(
                "max_clients above 1 requires psram: with ignore_not_found: false"
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicrophoneHttpStream),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.GenerateID(CONF_WEB_SERVER_ID): cv.use_id(web_server.WebServer),
            cv.Required(CONF_STREAM_ID): validate_stream_id,
            cv.Optional(CONF_MAX_CLIENTS, default=1): cv.int_range(min=1, max=3),
            cv.Required(CONF_SOURCE): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                min_channels=1,
                max_channels=1,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _register_sockets,
)

FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(
                CONF_SOURCE
            ): microphone.final_validate_microphone_source_schema(DOMAIN),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _validate_psram,
)


async def to_code(config: ConfigType) -> None:
    source = await microphone.microphone_source_to_code(
        config[CONF_SOURCE], passive=False
    )
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    server = await cg.get_variable(config[CONF_WEB_SERVER_ID])
    # Five regular web clients plus the configured long-lived audio clients.
    cg.add(base.set_min_open_sockets(5 + config[CONF_MAX_CLIENTS]))
    var = cg.new_Pvariable(
        config[CONF_ID],
        source,
        base,
        server,
        f"{STREAM_PATH_PREFIX}{config[CONF_STREAM_ID]}",
        config[CONF_MAX_CLIENTS],
    )
    await cg.register_component(var, config)
