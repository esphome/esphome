import esphome.codegen as cg
from esphome.components import network, socket
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect
import esphome.config_validation as cv
from esphome.const import CONF_CHANNELS, CONF_ID, CONF_METHOD, CONF_NAME
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["network"]
DOMAIN = "e131"

e131_ns = cg.esphome_ns.namespace("e131")
E131AddressableLightEffect = e131_ns.class_(
    "E131AddressableLightEffect", AddressableLightEffect
)
E131Component = e131_ns.class_("E131Component", cg.Component)

E131ListenMethod = e131_ns.enum("E131ListenMethod")
METHODS = E131ListenMethod.options("UNICAST", "MULTICAST", prefix="E131_")

E131LightChannels = e131_ns.enum("E131LightChannels")
CHANNELS = E131LightChannels.options("MONO", "RGB", "RGBW", prefix="E131_")

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
    ).extend(cv.COMPONENT_SCHEMA),
    network.require_ipv4("e131"),
    socket.consume_sockets(1, "e131", socket.SocketType.UDP),
)


async def to_code(config: ConfigType) -> None:
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
async def e131_light_effect_to_code(config: ConfigType, effect_id: ID) -> MockObj:
    parent = await cg.get_variable(config[CONF_E131_ID])

    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(effect.set_first_universe(config[CONF_UNIVERSE]))
    cg.add(effect.set_channels(CHANNELS[config[CONF_CHANNELS]]))
    cg.add(effect.set_e131(parent))
    return effect
