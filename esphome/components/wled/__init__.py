import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light.types import AddressableLightEffect
from esphome.components.light.effects import register_addressable_effect
from esphome.const import CONF_NAME, CONF_PORT

wled_ns = cg.esphome_ns.namespace("wled")
WLEDLightEffect = wled_ns.class_("WLEDLightEffect", AddressableLightEffect)

CONFIG_SCHEMA = cv.All(cv.Schema({}), cv.only_with_arduino)


@register_addressable_effect(
    "wled",
    WLEDLightEffect,
    "WLED",
    {
        cv.Optional(CONF_PORT, default=21324): cv.port,
    },
)
async def wled_light_effect_to_code(config, effect_id):
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_port(config[CONF_PORT]))

    return effect
