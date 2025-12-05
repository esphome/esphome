"""Sendspin Media Player Setup."""

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

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

# media_player is needed to control the stream
AUTO_LOAD = ["audio", "media_player"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

CONF_CONTROL_ONLY = "control_only"
CONF_ON_SERVER_SETTINGS = "on_server_settings"
CONF_VOLUME_MIN = "volume_min"
CONF_VOLUME_MAX = "volume_max"


SendspinMediaPlayer = sendspin_ns.class_(
    "SendspinMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)


# TODO: This can't be the best way. Eventually, the Sendspin media player should only control the group and not play audio at all. Actual audio playback should be handled by the speaker media player (but I need to build a MediaSource abstraction layer first...)
def _final_validate(config):
    """Build the appropriate schema based on control_only setting."""
    base_schema = media_player.media_player_schema(SendspinMediaPlayer).extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinMediaPlayer),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Optional(CONF_CONTROL_ONLY, default=False): cv.boolean,
            cv.Optional(CONF_ON_SERVER_SETTINGS): automation.validate_automation(
                single=True
            ),
        }
    )

    # If control_only is False (default), add audio-related fields
    if not config.get(CONF_CONTROL_ONLY, False):
        audio_schema = base_schema.extend(
            {
                cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
                cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                    cv.boolean, cv.only_with_esp_idf
                ),
                cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
                cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            }
        )
        return audio_schema(config)

    # For control_only=True, just use base schema
    return base_schema(config)


CONFIG_SCHEMA = cv.All(
    _final_validate,
    cv.only_on([PLATFORM_ESP32]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await media_player.register_media_player(var, config)

    if not config.get(CONF_CONTROL_ONLY, False):
        cg.add_define("USE_SENDSPIN_PLAYER")

        spkr = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(spkr))

        cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))
        cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))

        if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
            cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
            if task_stack_in_psram:
                esp32.add_idf_sdkconfig_option(
                    "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
                )

    return var
