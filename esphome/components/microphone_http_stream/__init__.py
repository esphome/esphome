"""Stream one microphone source as raw PCM over the shared web server."""

import re

import esphome.codegen as cg
from esphome.components import (
    microphone,
    network,
    psram,
    web_server_base,
    web_server_idf,
)
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISABLED,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SOURCE,
    CONF_WIFI,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mbrudka"]
AUTO_LOAD = ["ring_buffer", "web_server_base"]
DEPENDENCIES = ["microphone"]
DOMAIN = "microphone_http_stream"

CONF_STREAM_ID = "stream_id"
CONF_MAX_CLIENTS = "max_clients"
# Keep in sync with STREAM_PATH_PREFIX in microphone_http_stream.h
STREAM_PATH_PREFIX = "/audio/microphones/"
STREAM_ID_MAX_LEN = 32
_STREAM_ID_RE = re.compile(rf"[a-z_][a-z0-9_-]{{0,{STREAM_ID_MAX_LEN - 1}}}")

microphone_http_stream_ns = cg.esphome_ns.namespace(DOMAIN)
MicrophoneHttpStream = microphone_http_stream_ns.class_(
    "MicrophoneHttpStream", cg.Component
)
ClientSlot = microphone_http_stream_ns.struct("ClientSlot")


def validate_stream_id(value: str) -> str:
    if not _STREAM_ID_RE.fullmatch(value):
        raise cv.Invalid(
            f"stream_id must be 1-{STREAM_ID_MAX_LEN} lowercase ASCII letters, digits, _ or -,"
            " starting with a letter or _"
        )
    return value


def stream_id_from_microphone_id(microphone_id: str) -> str:
    """Default stream_id: the microphone id, which already passed cv.validate_id_name, lowercased."""
    return microphone_id.lower()[:STREAM_ID_MAX_LEN]


def _register_httpd_clients(config: ConfigType) -> ConfigType:
    web_server_base.add_httpd_clients(config[CONF_MAX_CLIENTS], DOMAIN)
    return config


def _require_plain_http(config: ConfigType) -> ConfigType:
    # AsyncStreamResponse answers 501 on an HTTPS server
    web_server_idf.require_plain_http(DOMAIN)
    return config


def _request_wifi_networking(config: ConfigType) -> ConfigType:
    # The default TCP send buffer is too small for a stream over Wi-Fi; Ethernet streams fine on it
    if CONF_WIFI in fv.full_config.get():
        network.require_high_performance_networking()
    return config


def _validate_psram(config: ConfigType) -> ConfigType:
    # A board that turns out to have no PSRAM fails the allocation at setup and answers 503
    if config[CONF_MAX_CLIENTS] > 1 and (
        (psram_config := fv.full_config.get().get(psram.DOMAIN)) is None
        or psram_config.get(CONF_DISABLED, False)
    ):
        raise cv.Invalid("max_clients above 1 requires enabled psram:")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicrophoneHttpStream),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_STREAM_ID): cv.All(cv.string_strict, validate_stream_id),
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
    _register_httpd_clients,
    _require_plain_http,
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
    _request_wifi_networking,
)


async def to_code(config: ConfigType) -> None:
    source = await microphone.microphone_source_to_code(
        config[CONF_SOURCE], passive=False
    )
    if (stream_id := config.get(CONF_STREAM_ID)) is None:
        stream_id = stream_id_from_microphone_id(
            config[CONF_SOURCE][CONF_MICROPHONE].id
        )
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    clients = cg.static_buffer(config[CONF_ID], ClientSlot, config[CONF_MAX_CLIENTS])
    var = cg.new_Pvariable(
        config[CONF_ID],
        source,
        base,
        f"{STREAM_PATH_PREFIX}{stream_id}",
        clients,
    )
    await cg.register_component(var, config)
