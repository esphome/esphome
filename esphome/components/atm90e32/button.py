import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_SCALE
from esphome.types import ConfigType

from .sensor import ATM90E32Component

CONF_RUN_GAIN_CALIBRATION = "run_gain_calibration"
CONF_CLEAR_GAIN_CALIBRATION = "clear_gain_calibration"
CONF_RUN_OFFSET_CALIBRATION = "run_offset_calibration"
CONF_CLEAR_OFFSET_CALIBRATION = "clear_offset_calibration"
CONF_RUN_POWER_OFFSET_CALIBRATION = "run_power_offset_calibration"
CONF_CLEAR_POWER_OFFSET_CALIBRATION = "clear_power_offset_calibration"


def _calibration_button(icon: str) -> cv.Schema:
    return button.button_schema(
        button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG, icon=icon
    )


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.use_id(ATM90E32Component),
    cv.Optional(CONF_RUN_GAIN_CALIBRATION): _calibration_button("mdi:scale-balance"),
    cv.Optional(CONF_CLEAR_GAIN_CALIBRATION): _calibration_button("mdi:delete"),
    cv.Optional(CONF_RUN_OFFSET_CALIBRATION): _calibration_button(ICON_SCALE),
    cv.Optional(CONF_CLEAR_OFFSET_CALIBRATION): _calibration_button("mdi:delete"),
    cv.Optional(CONF_RUN_POWER_OFFSET_CALIBRATION): _calibration_button(ICON_SCALE),
    cv.Optional(CONF_CLEAR_POWER_OFFSET_CALIBRATION): _calibration_button("mdi:delete"),
}


def _press(action: str, hint: str | None = None, hint2: str | None = None) -> str:
    """press_calibration_button() logs at most two hint lines."""
    args = "".join(
        f", {cg.LogStringLiteral(h)}" for h in (hint, hint2) if h is not None
    )
    return f"press_calibration_button(entity, &{ATM90E32Component}::{action}{args})"


async def to_code(config: ConfigType) -> None:
    buttons = button.sub_buttons(config, parent=config[CONF_ID])
    await buttons(
        CONF_RUN_GAIN_CALIBRATION,
        None,
        apply=_press(
            "run_gain_calibrations",
            "[CALIBRATION] Use gain_ct: & gain_voltage: under each phase_x: in your config file to save these values",
        ),
    )
    await buttons(
        CONF_CLEAR_GAIN_CALIBRATION, None, apply=_press("clear_gain_calibrations")
    )
    await buttons(
        CONF_RUN_OFFSET_CALIBRATION,
        None,
        apply=_press(
            "run_offset_calibrations",
            "[CALIBRATION] **NOTE: CTs and ACVs must be 0 during this process. USB power only**",
            "[CALIBRATION] Use offset_voltage: & offset_current: under each phase_x: in your config file to save these values",
        ),
    )
    await buttons(
        CONF_CLEAR_OFFSET_CALIBRATION, None, apply=_press("clear_offset_calibrations")
    )
    await buttons(
        CONF_RUN_POWER_OFFSET_CALIBRATION,
        None,
        apply=_press(
            "run_power_offset_calibrations",
            "[CALIBRATION] **NOTE: CTs must be 0 during this process. Voltage reference should be present**",
            "[CALIBRATION] Use offset_active_power: & offset_reactive_power: under each phase_x: in your config file to save these values",
        ),
    )
    await buttons(
        CONF_CLEAR_POWER_OFFSET_CALIBRATION,
        None,
        apply=_press("clear_power_offset_calibrations"),
    )
