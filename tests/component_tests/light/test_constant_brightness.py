"""Tests for constant_brightness of the cwww and rgbww lights: a boolean or color_temperature_priority."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.light import (
    CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY,
    CONSTANT_BRIGHTNESS_MODES,
    validate_constant_brightness,
)
from esphome.core import CORE


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (True, True),
        (False, False),
        ("yes", True),
        ("off", False),
        ("color_temperature_priority", CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY),
        ("COLOR_TEMPERATURE_PRIORITY", CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY),
    ],
)
def test_accepted_values(value: object, expected: object) -> None:
    result = validate_constant_brightness(value)
    assert result == expected
    assert result in CONSTANT_BRIGHTNESS_MODES


def test_other_values_rejected() -> None:
    with pytest.raises(cv.Invalid):
        validate_constant_brightness("sometimes")


def test_codegen_sets_mode(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("constant_brightness.yaml"))

    assert (
        "priority_output->set_constant_brightness("
        "light::ConstantBrightness::CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY);"
        in main_cpp
    )
    assert (
        "enabled_output->set_constant_brightness("
        "light::ConstantBrightness::CONSTANT_BRIGHTNESS_ENABLED);" in main_cpp
    )
    assert (
        "plain_output->set_constant_brightness("
        "light::ConstantBrightness::CONSTANT_BRIGHTNESS_DISABLED);" in main_cpp
    )
    assert (
        "rgbww_output->set_constant_brightness("
        "light::ConstantBrightness::CONSTANT_BRIGHTNESS_ENABLED);" in main_cpp
    )
    assert "USE_LIGHT_COLOR_TEMPERATURE_PRIORITY" in {d.name for d in CORE.defines}


def test_priority_code_only_when_used(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("constant_brightness_plain.yaml"))

    assert "USE_LIGHT_COLOR_TEMPERATURE_PRIORITY" not in {d.name for d in CORE.defines}
