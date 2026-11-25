import esphome.codegen as cg
from esphome.components import socket
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_PORT
from esphome.core import ID
from esphome.types import ConfigType

DOMAIN = "wled"
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]

wled_ns = cg.esphome_ns.namespace("wled")
WLEDLightEffect = wled_ns.class_("WLEDLightEffect", AddressableLightEffect)

CONFIG_SCHEMA = cv.Schema({})
CONF_SYNC_GROUP_MASK = "sync_group_mask"
CONF_BLANK_ON_START = "blank_on_start"


@register_addressable_effect(
    "wled",
    WLEDLightEffect,
    "WLED",
    {
        cv.Optional(CONF_PORT, default=21324): cv.port,
        cv.Optional(CONF_SYNC_GROUP_MASK, default=0): cv.uint8_t,
        cv.Optional(CONF_BLANK_ON_START, default=True): cv.boolean,
    },
    socket.consume_sockets(1, "wled", socket.SocketType.UDP),
)
async def wled_light_effect_to_code(config: ConfigType, effect_id: ID) -> cg.MockObj:
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_port(config[CONF_PORT]))
    cg.add(effect.set_sync_group_mask(config[CONF_SYNC_GROUP_MASK]))
    cg.add(effect.set_blank_on_start(config[CONF_BLANK_ON_START]))
    return effect
