import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL

from .. import ROJAFLEX_DEVICE_SCHEMA, RojaflexDevice, register_rojaflex_device, rojaflex_ns

DEPENDENCIES = ["rojaflex"]

RojaflexCover = rojaflex_ns.class_(
    "RojaflexCover", cover.Cover, cg.Component, RojaflexDevice
)

CONFIG_SCHEMA = (
    cover.cover_schema(RojaflexCover)
    .extend(ROJAFLEX_DEVICE_SCHEMA)
    .extend(
        {
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await register_rojaflex_device(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
