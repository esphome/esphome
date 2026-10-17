import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_ESPECTRE_ID, ESPectreComponent, espectre_ns

DEPENDENCIES = ["espectre"]

RecalibrateButton = espectre_ns.class_("RecalibrateButton", button.Button)

CONFIG_SCHEMA = button.button_schema(
    RecalibrateButton, entity_category=ENTITY_CATEGORY_CONFIG
).extend({cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent)})


async def to_code(config: ConfigType) -> None:
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_ESPECTRE_ID])
