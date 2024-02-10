import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light.types import AddressableLightEffect
from esphome.components.light.effects import register_addressable_effect
from esphome.const import CONF_NAME, CONF_PORT

wled_ns = cg.esphome_ns.namespace("wled")
WLEDLightEffect = wled_ns.class_("WLEDLightEffect", AddressableLightEffect)

CONFIG_SCHEMA = cv.All(cv.Schema({}), cv.only_with_arduino)
CONF_SYNC_GROUP_MASK = "sync_group_mask"
CONF_BLANK_ON_START = "blank_on_start"


@register_addressable_effect(
    "wled",
    WLEDLightEffect,
    "WLED",
    {
        cv.Optional(CONF_PORT, default=21324): cv.port,
        cv.Optional(CONF_SYNC_GROUP_MASK, default=0): cv.int_range(min=0, max=255),
        cv.Optional(CONF_BLANK_ON_START, default=True): cv.boolean,
    },
)
async def wled_light_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_port(config[CONF_PORT]))
    cg.add(effect.set_sync_group_mask(config[CONF_SYNC_GROUP_MASK]))
    cg.add(effect.set_blank_on_start(config[CONF_BLANK_ON_START]))
    return effect
