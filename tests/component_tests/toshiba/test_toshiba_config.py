"""Tests for Toshiba model selection and power-selection binding."""

import pytest

from esphome.components.toshiba import climate, select
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("model", list(climate.MODELS))
def test_model_schema(model: str, set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = climate.CONFIG_SCHEMA({"name": "Toshiba", CONF_MODEL: model.lower()})
    assert config[CONF_MODEL] == model


def test_default_model_is_unchanged(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = climate.CONFIG_SCHEMA({"name": "Toshiba"})
    assert config[CONF_MODEL] == "GENERIC"


@pytest.mark.parametrize("model", list(climate.MODELS))
def test_power_selection_checks_the_referenced_climate(
    model: str, set_core_config: SetCoreConfigCallable
) -> None:
    full = Config()
    full["climate"] = [
        {CONF_ID: ID("selected", is_declaration=True), CONF_MODEL: model},
        {
            CONF_ID: ID("other", is_declaration=True),
            CONF_MODEL: "RAS-25J2KVSG-ND",
        },
    ]
    for index, config in enumerate(full["climate"]):
        full.declare_ids.append((config[CONF_ID], ["climate", index, CONF_ID]))
    set_core_config(PlatformFramework.ESP8266_ARDUINO, full_config=full)
    config: ConfigType = select.CONFIG_SCHEMA(
        {
            select.CONF_TOSHIBA_ID: "selected",
            select.CONF_POWER_SELECTION: {"name": "Power selection"},
        }
    )
    if model == "RAS-25J2KVSG-ND":
        assert select.FINAL_VALIDATE_SCHEMA(config) is config
    else:
        with pytest.raises(cv.Invalid, match="requires model RAS-25J2KVSG-ND"):
            select.FINAL_VALIDATE_SCHEMA(config)
