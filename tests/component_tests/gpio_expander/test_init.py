"""Tests for the shared io expander interrupt_pin validator."""

from __future__ import annotations

import importlib

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.gpio_expander import validate_interrupt_pin
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture
def stage_esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )


def test_plain_pin_accepted(stage_esp32: None) -> None:
    value = validate_interrupt_pin(
        {"number": 16, "mode": {"input": True, "pullup": True}}
    )
    assert value["number"] == 16


def test_inverted_rejected(stage_esp32: None) -> None:
    with pytest.raises(cv.Invalid, match="'inverted: true' is not supported"):
        validate_interrupt_pin({"number": 16, "inverted": True})


def test_allow_other_uses_rejected(stage_esp32: None) -> None:
    with pytest.raises(cv.Invalid, match="'allow_other_uses: true' is not supported"):
        validate_interrupt_pin({"number": 16, "allow_other_uses": True})


# mcp23017 covers the shared mcp23xxx_base schema
@pytest.mark.parametrize(
    "component",
    [
        "pcf8574",
        "pca9554",
        "tca9555",
        "pca6416a",
        "pi4ioe5v6408",
        "mcp23016",
        "mcp23017",
    ],
)
def test_component_schemas_route_through_validator(
    stage_esp32: None, component: str
) -> None:
    module = importlib.import_module(f"esphome.components.{component}")
    with pytest.raises(cv.Invalid, match="'inverted: true' is not supported"):
        module.CONFIG_SCHEMA(
            {"id": "expander_hub", "interrupt_pin": {"number": 16, "inverted": True}}
        )
