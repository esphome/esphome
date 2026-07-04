import esphome.codegen as cg
from esphome.components import media_player
from esphome.components.const import CONF_VOLUME_INCREMENT
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CONF_SENDSPIN_ID, SendspinHub, request_controller_support, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinMediaPlayer = sendspin_ns.class_(
    "SendspinMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)


def _request_roles(config: ConfigType) -> ConfigType:
    """Request the necessary Sendspin roles for the media player."""
    request_controller_support()

    return config


CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(SendspinMediaPlayer).extend(
        {
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
        }
    ),
    cv.only_on_esp32,
    _request_roles,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await media_player.register_media_player(var, config)

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
