import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.light.types import AddressableLightEffect
from esphome.components.light.effects import register_addressable_effect
from esphome.const import CONF_ID, CONF_NAME, CONF_METHOD, CONF_CHANNELS

DEPENDENCIES = ["network"]

e131_ns = cg.esphome_ns.namespace("e131")
E131AddressableLightEffect = e131_ns.class_(
    "E131AddressableLightEffect", AddressableLightEffect
)
E131Component = e131_ns.class_("E131Component", cg.Component)

METHODS = {"UNICAST": e131_ns.E131_UNICAST, "MULTICAST": e131_ns.E131_MULTICAST}

CHANNELS = {
    "MONO": e131_ns.E131_MONO,
    "RGB": e131_ns.E131_RGB,
    "RGBW": e131_ns.E131_RGBW,
}

CONF_UNIVERSE = "universe"
CONF_E131_ID = "e131_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(E131Component),
            cv.Optional(CONF_METHOD, default="MULTICAST"): cv.one_of(
                *METHODS, upper=True
            ),
        }
    ),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_method(METHODS[config[CONF_METHOD]]))


@register_addressable_effect(
    "e131",
    E131AddressableLightEffect,
    "E1.31",
    {
        cv.GenerateID(CONF_E131_ID): cv.use_id(E131Component),
        cv.Required(CONF_UNIVERSE): cv.int_range(min=1, max=512),
        cv.Optional(CONF_CHANNELS, default="RGB"): cv.one_of(*CHANNELS, upper=True),
    },
)
async def e131_light_effect_to_code(config, effect_id):
    parent = await cg.get_variable(config[CONF_E131_ID])

    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_first_universe(config[CONF_UNIVERSE]))
    cg.add(effect.set_channels(CHANNELS[config[CONF_CHANNELS]]))
    cg.add(effect.set_e131(parent))
    return effect
