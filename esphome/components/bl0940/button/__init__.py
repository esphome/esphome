import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_RESTART

from .. import CONF_BL0940_ID, bl0940_ns
from ..sensor import BL0940

CalibrationResetButton = bl0940_ns.class_(
    "CalibrationResetButton", button.Button, cg.Component
)

CONFIG_SCHEMA = cv.All(
    button.button_schema(
        CalibrationResetButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART,
    )
    .extend({cv.GenerateID(CONF_BL0940_ID): cv.use_id(BL0940)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_BL0940_ID])
