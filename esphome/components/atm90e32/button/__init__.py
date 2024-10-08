import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_CHIP, ICON_SCALE

from .. import atm90e32_ns
from ..sensor import ATM90E32Component

CONF_RUN_OFFSET_CALIBRATION = "run_offset_calibration"
CONF_CLEAR_OFFSET_CALIBRATION = "clear_offset_calibration"

ATM90E32CalibrationButton = atm90e32_ns.class_(
    "ATM90E32CalibrationButton",
    button.Button,
)
ATM90E32ClearCalibrationButton = atm90e32_ns.class_(
    "ATM90E32ClearCalibrationButton",
    button.Button,
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.use_id(ATM90E32Component),
    cv.Optional(CONF_RUN_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32CalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
    cv.Optional(CONF_CLEAR_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32ClearCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_CHIP,
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])
    if run_offset := config.get(CONF_RUN_OFFSET_CALIBRATION):
        b = await button.new_button(run_offset)
        await cg.register_parented(b, parent)
    if clear_offset := config.get(CONF_CLEAR_OFFSET_CALIBRATION):
        b = await button.new_button(clear_offset)
        await cg.register_parented(b, parent)
