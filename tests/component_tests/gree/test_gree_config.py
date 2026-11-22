"""Tests for gree climate direction-default configuration."""

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
    ],
)
def test_schema_accepts_all_builtin_models(
    model: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """All built-in Gree climate models accept direction defaults."""
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


def test_direction_defaults_to_auto(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Omitting direction defaults leaves the vanes at auto."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.gree.climate import CONFIG_SCHEMA

    validated = CONFIG_SCHEMA(
        {
            "name": "Gree YAC",
            "model": "yac",
        }
    )

    assert str(validated["horizontal_default"]) == "auto"
    assert str(validated["vertical_default"]) == "auto"


def test_schema_rejects_invalid_direction_values(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Direction defaults are constrained to known enum values."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.gree.climate import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="Unknown value 'invalid'"):
        CONFIG_SCHEMA(
            {
                "name": "Gree YAC Invalid",
                "model": "yac",
                "vertical_default": "invalid",
            }
        )
