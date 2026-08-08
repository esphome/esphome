import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_HOERMANN_ID, Hoermann, hoermann_ns

DEPENDENCIES = ["hoermann"]

HoermannCover = hoermann_ns.class_("HoermannCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = (
    cover.cover_schema(HoermannCover)
    .extend({cv.GenerateID(CONF_HOERMANN_ID): cv.use_id(Hoermann)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_ID])
    var = await cover.new_cover(config, parent)
    await cg.register_component(var, config)
