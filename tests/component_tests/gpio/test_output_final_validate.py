"""Tests for GPIO output final validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_HOLD_STATE
from esphome.components.gpio.output import FINAL_VALIDATE_SCHEMA
from esphome.config import Config
from esphome.const import CONF_ID, CONF_PIN, CONF_POWER_SUPPLY, PlatformFramework
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("power_supply_holds", [False, True])
def test_output_hold_state_requires_power_supply_hold_state(
    set_core_config: SetCoreConfigCallable,
    power_supply_holds: bool,
) -> None:
    supply_id = ID("supply", is_declaration=True, type="power_supply")
    full_config = Config()
    full_config["power_supply"] = [
        {CONF_ID: supply_id, CONF_PIN: {CONF_HOLD_STATE: power_supply_holds}}
    ]
    full_config.declare_ids.append((supply_id, ["power_supply", 0, CONF_ID]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    config = {
        CONF_PIN: {CONF_HOLD_STATE: True},
        CONF_POWER_SUPPLY: ID("supply", is_declaration=False, type="power_supply"),
    }
    if power_supply_holds:
        FINAL_VALIDATE_SCHEMA(config)
    else:
        with pytest.raises(
            cv.Invalid,
            match="The power supply pin must also set hold_state when an output it powers sets it",
        ):
            FINAL_VALIDATE_SCHEMA(config)
