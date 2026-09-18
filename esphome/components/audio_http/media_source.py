from pathlib import Path

import esphome.codegen as cg
from esphome.components import audio, media_source, psram
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TASK_STACK_IN_PSRAM
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@kahrendt"]
AUTO_LOAD = ["audio"]

CONF_PERSISTENT_RING_BUFFER = "persistent_ring_buffer"
CONF_CA_CERTIFICATE_PATH = "ca_certificate_path"
CONF_HTTP_REQUEST = "http_request"

audio_http_ns = cg.esphome_ns.namespace("audio_http")
AudioHTTPMediaSource = audio_http_ns.class_(
    "AudioHTTPMediaSource", cg.Component, media_source.MediaSource
)


def _request_micro_decoder(config: ConfigType) -> ConfigType:
    audio.request_micro_decoder_support()
    return config


def _default_ca_certificate_path(config: ConfigType) -> ConfigType:
    # Default to the CA certificate configured on the http_request component so
    # HTTPS playback verifies against the same trust anchor without repeating
    # the option on every source. Needed because audio_http sources are often
    # declared by device packages and cannot be extended from the device config.
    if CONF_CA_CERTIFICATE_PATH in config:
        return config
    http_request_config = (CORE.raw_config or {}).get(CONF_HTTP_REQUEST)
    if isinstance(http_request_config, dict) and CONF_CA_CERTIFICATE_PATH in http_request_config:
        config[CONF_CA_CERTIFICATE_PATH] = cv.file_(
            http_request_config[CONF_CA_CERTIFICATE_PATH]
        )
    return config


CONFIG_SCHEMA = cv.All(
    media_source.media_source_schema(
        AudioHTTPMediaSource,
    )
    .extend(
        {
            cv.Optional(CONF_BUFFER_SIZE, default=50000): cv.int_range(
                min=5000, max=1000000
            ),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_PERSISTENT_RING_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_CA_CERTIFICATE_PATH): cv.file_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _request_micro_decoder,
    _default_ca_certificate_path,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)
    
    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()
        
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_persistent_ring_buffer(config[CONF_PERSISTENT_RING_BUFFER]))
    
    # Embed the certificate content, like http_request does. Passed to
    # micro_decoder's DecoderConfig::http_ca_certificate, which then uses it as
    # the sole trust anchor for HTTPS playback URLs instead of the certificate
    # bundle.
    if ca_cert_path := config.get(CONF_CA_CERTIFICATE_PATH):
        with Path(ca_cert_path).open(encoding="utf-8") as f:
            ca_cert_content = f.read()
        cg.add(var.set_http_ca_certificate(ca_cert_content))
        
