"""Resonate Media Player Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, media_player, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SPEAKER,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)

from .. import CONF_RESONATE_ID, ResonateHub, resonate_ns

# media_player is needed to control the stream
AUTO_LOAD = ["audio", "media_player"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["resonate", "speaker"]

CONF_ON_SERVER_SETTINGS = "on_server_settings"

ResonateMediaPlayer = resonate_ns.class_(
    "ResonateMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)


CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(ResonateMediaPlayer).extend(
        {
            cv.GenerateID(): cv.declare_id(ResonateMediaPlayer),
            cv.GenerateID(CONF_RESONATE_ID): cv.use_id(ResonateHub),
            cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_ON_SERVER_SETTINGS): automation.validate_automation(
                single=True
            ),
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
)


async def to_code(config):
    cg.add_define("USE_RESONATE_AUDIO")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_RESONATE_ID])
    await media_player.register_media_player(var, config)

    spkr = await cg.get_variable(config[CONF_SPEAKER])
    cg.add(var.set_speaker(spkr))

    if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
        if task_stack_in_psram:
            esp32.add_idf_sdkconfig_option(
                "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
            )

    return var
