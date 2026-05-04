"""Tests for gree climate configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "model",
    [
        "generic",
        "yan",
        "yaa",
        "yac",
        "yac1fb9",
        "yx1ff",
        "yag",
        "yap1f",
    ],
)
def test_schema_accepts_all_builtin_models(
    model: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """All built-in Gree climate models are accepted."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.gree.climate import CONFIG_SCHEMA

    validated = CONFIG_SCHEMA(
        {
            "name": f"Gree {model.upper()}",
            "model": model,
            "receiver_id": "rcvr",
            "horizontal_default": "middle",
            "vertical_default": "up",
        }
    )

    assert str(validated["model"]) == model
    assert str(validated["horizontal_default"]) == "middle"
    assert str(validated["vertical_default"]) == "up"
    assert "receiver_id" in validated


def test_switch_supported_models_match_builtin_gree_models() -> None:
    """Switch light/turbo/health/xfan support keeps parity with built-in models."""
    from esphome.components.gree.climate import MODELS
    from esphome.components.gree.switch import SUPPORTED_MODELS

    assert set(MODELS) == set(SUPPORTED_MODELS)


def test_yap1f_schema_rejects_invalid_direction_values(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Direction defaults are constrained to known enum values."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.gree.climate import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="Unknown value 'invalid'"):
        CONFIG_SCHEMA(
            {
                "name": "Gree YAP1F Invalid",
                "model": "yap1f",
                "vertical_default": "invalid",
            }
        )
