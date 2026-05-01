import esphome.codegen as cg
from esphome.components import media_source, psram
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TASK_STACK_IN_PSRAM
from esphome.types import ConfigType

CODEOWNERS = ["@kahrendt"]
AUTO_LOAD = ["audio"]
DEPENDENCIES = ["audio_file"]

audio_file_ns = cg.esphome_ns.namespace("audio_file")
AudioFileMediaSource = audio_file_ns.class_(
    "AudioFileMediaSource", cg.Component, media_source.MediaSource
)

CONFIG_SCHEMA = cv.All(
    media_source.media_source_schema(
        AudioFileMediaSource,
    )
    .extend(
        {
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.All(
                cv.boolean, cv.requires_component(psram.DOMAIN)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_source.register_media_source(var, config)

    if CONF_TASK_STACK_IN_PSRAM in config:
        cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
