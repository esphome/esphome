import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv

from . import ECOCOMFORT2_CLIENT_SCHEMA, Ecocomfort2Fan, register_ecocomfort2_child

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = (
    fan.fan_schema(Ecocomfort2Fan)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECOCOMFORT2_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await register_ecocomfort2_child(var, config)
