"""Tests for heatpumpir climate configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.const import CONF_LIGHT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_greeyap_allows_light_option(set_core_config: SetCoreConfigCallable) -> None:
    """Greeyap supports configuring the light flag."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.heatpumpir.climate import CONFIG_SCHEMA

    validated = CONFIG_SCHEMA(
        {
            "name": "HeatpumpIR Gree YAP",
            "protocol": "greeyap",
            "horizontal_default": "auto",
            "vertical_default": "up",
            "min_temperature": 16,
            "max_temperature": 30,
            "light": False,
        }
    )
    assert validated[CONF_LIGHT] is False

    # Other protocols still validate fine as long as `light` is not provided.
    CONFIG_SCHEMA(
        {
            "name": "HeatpumpIR Gree YAC",
            "protocol": "greeyac",
            "horizontal_default": "auto",
            "vertical_default": "up",
            "min_temperature": 16,
            "max_temperature": 30,
        }
    )


def test_vaillant_allows_light_option(set_core_config: SetCoreConfigCallable) -> None:
    """Vaillantvai8 supports configuring the light flag."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.heatpumpir.climate import CONFIG_SCHEMA

    validated = CONFIG_SCHEMA(
        {
            "name": "HeatpumpIR Vaillant",
            "protocol": "vaillantvai8",
            "horizontal_default": "auto",
            "vertical_default": "up",
            "min_temperature": 16,
            "max_temperature": 30,
            "light": False,
        }
    )
    assert validated[CONF_LIGHT] is False


def test_non_light_capable_protocol_rejects_light_option(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Only light-capable protocols accept the light flag in heatpumpir."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    from esphome.components.heatpumpir.climate import CONFIG_SCHEMA

    with pytest.raises(
        cv.Invalid,
        match=r"light is only configurable for protocols greeyap, vaillantvai8 in heatpumpir",
    ):
        CONFIG_SCHEMA(
            {
                "name": "HeatpumpIR Gree YAC Light",
                "protocol": "greeyac",
                "horizontal_default": "auto",
                "vertical_default": "up",
                "min_temperature": 16,
                "max_temperature": 30,
                "light": False,
            }
        )
