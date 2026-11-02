import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import ECOCOMFORT2_CLIENT_SCHEMA, ecocomfort2_ns, register_ecocomfort2_child

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

Ecocomfort2Fan = ecocomfort2_ns.class_("Ecocomfort2Fan", fan.Fan, cg.Component)

CONFIG_SCHEMA = (
    fan.fan_schema(Ecocomfort2Fan)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECOCOMFORT2_CLIENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await register_ecocomfort2_child(var, config)
