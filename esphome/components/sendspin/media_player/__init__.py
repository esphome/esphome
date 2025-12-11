"""Sendspin Media Player Setup."""

import esphome.codegen as cg
from esphome.components import media_player
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SPEAKER, CONF_TASK_STACK_IN_PSRAM

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

CONFIG_SCHEMA = media_player.media_player_schema(SendspinMediaPlayer).extend(
    {
        cv.GenerateID(): cv.declare_id(SendspinMediaPlayer),
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
        cv.Optional(CONF_CONTROL_ONLY): cv.invalid(
            f"The {CONF_CONTROL_ONLY} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
        cv.Optional(CONF_ON_SERVER_SETTINGS): cv.invalid(
            f"The {CONF_ON_SERVER_SETTINGS} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
        cv.Optional(CONF_SPEAKER): cv.invalid(
            f"The {CONF_SPEAKER} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
        cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.invalid(
            f"The {CONF_TASK_STACK_IN_PSRAM} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
        cv.Optional(CONF_VOLUME_MIN): cv.invalid(
            f"The {CONF_VOLUME_MIN} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
        cv.Optional(CONF_VOLUME_MAX): cv.invalid(
            f"The {CONF_VOLUME_MAX} option is deprecated, see https://github.com/esphome/esphome/pull/12284 for latest changes"
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await media_player.register_media_player(var, config)

    return var
