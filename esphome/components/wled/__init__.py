import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light.types import AddressableLightEffect
from esphome.components.light.effects import register_addressable_effect
from esphome.const import CONF_NAME, CONF_PORT

wled_ns = cg.esphome_ns.namespace("wled")
WLEDLightEffect = wled_ns.class_("WLEDLightEffect", AddressableLightEffect)

CONFIG_SCHEMA = cv.All(cv.Schema({}), cv.only_with_arduino)

CONF_SYNCGROUP_MASK = "sync_group_mask"

@register_addressable_effect(
    "wled",
    WLEDLightEffect,
    "WLED",
    {
        cv.Optional(CONF_PORT, default=21324): cv.port,
        cv.Optional(CONF_SYNCGROUP_MASK, default=0): cv.int_range(min=0, max=255)
    },
)
async def wled_light_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_port(config[CONF_PORT]))
    cg.add(effect.set_sync_group_mask(config[CONF_SYNCGROUP_MASK]));
    return effect
