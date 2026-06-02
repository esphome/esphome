import esphome.codegen as cg
from esphome.components import audio, media_source, psram
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TASK_STACK_IN_PSRAM
from esphome.types import ConfigType

CODEOWNERS = ["@kahrendt"]
AUTO_LOAD = ["audio"]

audio_http_ns = cg.esphome_ns.namespace("audio_http")
AudioHTTPMediaSource = audio_http_ns.class_(
    "AudioHTTPMediaSource", cg.Component, media_source.MediaSource
)


def _request_micro_decoder(config: ConfigType) -> ConfigType:
    audio.request_micro_decoder_support()
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
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _request_micro_decoder,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
