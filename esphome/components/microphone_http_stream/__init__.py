"""Stream one microphone source as raw PCM over the shared web server."""

import re

import esphome.codegen as cg
from esphome.components import microphone, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SOURCE
from esphome.types import ConfigType

CODEOWNERS = ["@mbrudka"]
DEPENDENCIES = ["microphone", "web_server"]
DOMAIN = "microphone_http_stream"

CONF_STREAM_ID = "stream_id"
CONF_MAX_CLIENTS = "max_clients"
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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicrophoneHttpStream),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
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

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE): microphone.final_validate_microphone_source_schema(
            DOMAIN
        ),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    source = await microphone.microphone_source_to_code(
        config[CONF_SOURCE], passive=False
    )
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    # Five regular web clients plus the configured long-lived audio clients.
    cg.add(base.set_min_open_sockets(5 + config[CONF_MAX_CLIENTS]))
    var = cg.new_Pvariable(
        config[CONF_ID],
        source,
        base,
        config[CONF_STREAM_ID],
        config[CONF_MAX_CLIENTS],
    )
    await cg.register_component(var, config)
