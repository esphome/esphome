"""Sendspin Media Player Setup."""

import esphome.codegen as cg
from esphome.components import media_player
from esphome.components.const import CONF_VOLUME_INCREMENT
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinMediaPlayer = sendspin_ns.class_(
    "SendspinMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)

CONFIG_SCHEMA = media_player.media_player_schema(SendspinMediaPlayer).extend(
    {
        cv.GenerateID(): cv.declare_id(SendspinMediaPlayer),
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
        cv.Optional(CONF_VOLUME_INCREMENT, default=5): cv.int_range(min=1, max=50),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await media_player.register_media_player(var, config)

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))

    cg.add_define("USE_SENDSPIN_CONTROLLER")

    return var
