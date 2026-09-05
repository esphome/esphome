import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp, hoermann_hcp_ns

DEPENDENCIES = ["hoermann_hcp"]

HoermannHcpLight = hoermann_hcp_ns.class_(
    "HoermannHcpLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = (
    light.light_schema(HoermannHcpLight, light.LightType.BINARY)
    .extend({cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    var = await light.new_light(config, parent)
    await cg.register_component(var, config)
