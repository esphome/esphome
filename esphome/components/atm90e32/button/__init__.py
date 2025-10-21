import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_SCALE

from .. import atm90e32_ns
from ..sensor import ATM90E32Component

CONF_RUN_GAIN_CALIBRATION = "run_gain_calibration"
CONF_CLEAR_GAIN_CALIBRATION = "clear_gain_calibration"
CONF_RUN_OFFSET_CALIBRATION = "run_offset_calibration"
CONF_CLEAR_OFFSET_CALIBRATION = "clear_offset_calibration"
CONF_RUN_POWER_OFFSET_CALIBRATION = "run_power_offset_calibration"
CONF_CLEAR_POWER_OFFSET_CALIBRATION = "clear_power_offset_calibration"

ATM90E32GainCalibrationButton = atm90e32_ns.class_(
    "ATM90E32GainCalibrationButton", button.Button
)
ATM90E32ClearGainCalibrationButton = atm90e32_ns.class_(
    "ATM90E32ClearGainCalibrationButton", button.Button
)
ATM90E32OffsetCalibrationButton = atm90e32_ns.class_(
    "ATM90E32OffsetCalibrationButton", button.Button
)
ATM90E32ClearOffsetCalibrationButton = atm90e32_ns.class_(
    "ATM90E32ClearOffsetCalibrationButton", button.Button
)
ATM90E32PowerOffsetCalibrationButton = atm90e32_ns.class_(
    "ATM90E32PowerOffsetCalibrationButton", button.Button
)
ATM90E32ClearPowerOffsetCalibrationButton = atm90e32_ns.class_(
    "ATM90E32ClearPowerOffsetCalibrationButton", button.Button
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.use_id(ATM90E32Component),
    cv.Optional(CONF_RUN_GAIN_CALIBRATION): button.button_schema(
        ATM90E32GainCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:scale-balance",
    ),
    cv.Optional(CONF_CLEAR_GAIN_CALIBRATION): button.button_schema(
        ATM90E32ClearGainCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:delete",
    ),
    cv.Optional(CONF_RUN_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32OffsetCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
    cv.Optional(CONF_CLEAR_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32ClearOffsetCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:delete",
    ),
    cv.Optional(CONF_RUN_POWER_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32PowerOffsetCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
    cv.Optional(CONF_CLEAR_POWER_OFFSET_CALIBRATION): button.button_schema(
        ATM90E32ClearPowerOffsetCalibrationButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:delete",
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if run_gain := config.get(CONF_RUN_GAIN_CALIBRATION):
        b = await button.new_button(run_gain)
        await cg.register_parented(b, parent)

    if clear_gain := config.get(CONF_CLEAR_GAIN_CALIBRATION):
        b = await button.new_button(clear_gain)
        await cg.register_parented(b, parent)

    if run_offset := config.get(CONF_RUN_OFFSET_CALIBRATION):
        b = await button.new_button(run_offset)
        await cg.register_parented(b, parent)

    if clear_offset := config.get(CONF_CLEAR_OFFSET_CALIBRATION):
        b = await button.new_button(clear_offset)
        await cg.register_parented(b, parent)

    if run_power := config.get(CONF_RUN_POWER_OFFSET_CALIBRATION):
        b = await button.new_button(run_power)
        await cg.register_parented(b, parent)

    if clear_power := config.get(CONF_CLEAR_POWER_OFFSET_CALIBRATION):
        b = await button.new_button(clear_power)
        await cg.register_parented(b, parent)
