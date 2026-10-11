import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

HoermannHcpCover = hoermann_hcp_ns.class_("HoermannHcpCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = (
    cover.cover_schema(HoermannHcpCover)
    .extend({cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    var = await cover.new_cover(config, parent)
    await cg.register_component(var, config)
