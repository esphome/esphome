import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core import CORE
from esphome.types import ConfigType

from .. import (
    CONF_ESPECTRE_ID,
    DOMAIN,
    ESPectreComponent,
    supported_traffic_generator_modes,
)

DEPENDENCIES = ["espectre"]


CONFIG_SCHEMA = select.select_schema(
    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
).extend({cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent)})


async def to_code(config: ConfigType) -> None:
    options = supported_traffic_generator_modes(CORE.config[DOMAIN])
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    await select.sub_selects(config, parent=hub).create(
        config,
        hub.set_traffic_mode_select,
        options=options,
        apply="request_traffic_generator_mode(option)",
    )
