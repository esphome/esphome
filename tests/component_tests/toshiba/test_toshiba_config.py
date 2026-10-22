"""Tests for Toshiba model selection and power-selection binding."""

import pytest

from esphome.components.toshiba import climate, select
from esphome.config import Config, validate_config
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


@pytest.mark.parametrize("model", list(climate.MODELS))
@pytest.mark.parametrize("referenced_id", ["selected", "other"])
def test_fix_checks_only_the_referenced_climate(
    model: str, referenced_id: str, set_core_config: SetCoreConfigCallable
) -> None:
    full = Config()
    full["climate"] = [
        {CONF_ID: ID("selected", is_declaration=True), CONF_MODEL: model},
        {
            CONF_ID: ID("other", is_declaration=True),
            CONF_MODEL: climate.MODEL_RAS_25J2KVSG_ND,
        },
    ]
    full["script"] = [{"then": [{"toshiba.fix": {CONF_ID: ID(referenced_id)}}]}]
    set_core_config(PlatformFramework.ESP8266_ARDUINO, full_config=full)
    for config in full["climate"]:
        if (
            config[CONF_ID].id == referenced_id
            and config[CONF_MODEL] != climate.MODEL_RAS_25J2KVSG_ND
        ):
            with pytest.raises(cv.Invalid, match="toshiba.fix requires model") as error:
                climate.FINAL_VALIDATE_SCHEMA(config)
            assert error.value.path[-6:] == [
                "script",
                0,
                "then",
                0,
                "toshiba.fix",
                CONF_ID,
            ]
        else:
            assert climate.FINAL_VALIDATE_SCHEMA(config) is config


@pytest.mark.parametrize("model", list(climate.MODELS))
@pytest.mark.parametrize("simple", [False, True])
def test_fix_model_validation_in_full_config(model: str, simple: bool) -> None:
    result = validate_config(
        {
            "esphome": {"name": "toshiba-fix-test"},
            "esp8266": {"board": "d1_mini"},
            "remote_transmitter": {"pin": 4, "carrier_duty_percent": "50%"},
            "climate": [
                {"platform": "toshiba", "id": "selected", "model": model},
                {
                    "platform": "toshiba",
                    "id": "other",
                    "model": climate.MODEL_RAS_25J2KVSG_ND,
                },
            ],
            "script": [
                {
                    "id": "run_fix",
                    "then": [
                        {"toshiba.fix": "selected" if simple else {"id": "selected"}}
                    ],
                }
            ],
        },
        {},
    )
    if model == climate.MODEL_RAS_25J2KVSG_ND:
        assert not result.errors
    else:
        assert len(result.errors) == 1
        assert "toshiba.fix requires model" in str(result.errors[0])
        assert result.errors[0].path == ["script", 0, "then", 0, "toshiba.fix", CONF_ID]
