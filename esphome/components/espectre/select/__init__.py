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
    espectre_ns,
    supported_traffic_generator_modes,
)

DEPENDENCIES = ["espectre"]

TrafficModeSelect = espectre_ns.class_(
    "TrafficModeSelect", select.Select, cg.Parented.template(ESPectreComponent)
)

CONFIG_SCHEMA = select.select_schema(
    TrafficModeSelect, entity_category=ENTITY_CATEGORY_CONFIG
).extend({cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent)})


async def to_code(config: ConfigType) -> None:
    options = supported_traffic_generator_modes(CORE.config[DOMAIN])
    var = await select.new_select(config, options=options)
    await cg.register_parented(var, config[CONF_ESPECTRE_ID])
    parent = await cg.get_variable(config[CONF_ESPECTRE_ID])
    cg.add(parent.set_traffic_mode_select(var))
