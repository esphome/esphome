"""Tests for constant_brightness of the cwww and rgbww lights: a boolean or color_temperature_priority."""

from collections.abc import Callable
import math
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.light import (
    CONF_COLD_TO_WARM_RATIO,
    CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY,
    CONSTANT_BRIGHTNESS_MODES,
    CONSTANT_BRIGHTNESS_SCHEMA,
    validate_constant_brightness,
)
from esphome.const import CONF_CONSTANT_BRIGHTNESS
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


@pytest.mark.parametrize(
    "mode", [False, True, CONSTANT_BRIGHTNESS_COLOR_TEMPERATURE_PRIORITY]
)
def test_ratio_applies_in_every_mode(mode: object) -> None:
    config = {CONF_CONSTANT_BRIGHTNESS: mode, CONF_COLD_TO_WARM_RATIO: 0.71}
    assert CONSTANT_BRIGHTNESS_SCHEMA(config) == config


def test_ratio_default() -> None:
    assert CONSTANT_BRIGHTNESS_SCHEMA({})[CONF_COLD_TO_WARM_RATIO] == 1.0


@pytest.mark.parametrize("value", [0.01, 1, 100])
def test_ratio_bounds_accepted(value: float) -> None:
    config = CONSTANT_BRIGHTNESS_SCHEMA({CONF_COLD_TO_WARM_RATIO: value})
    assert config[CONF_COLD_TO_WARM_RATIO] == value


@pytest.mark.parametrize("value", [0, -1, 0.0099, 100.1, math.inf, -math.inf, math.nan])
def test_ratio_outside_range_rejected(value: float) -> None:
    with pytest.raises(cv.Invalid):
        CONSTANT_BRIGHTNESS_SCHEMA({CONF_COLD_TO_WARM_RATIO: value})


def _relative(main_cpp: str, output: str) -> tuple[float, float] | None:
    match = re.search(
        rf"{output}->set_white_relative_brightness\(([\d.e+-]+)f, ([\d.e+-]+)f\);",
        main_cpp,
    )
    return None if match is None else (float(match[1]), float(match[2]))


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
    assert "USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS" in {d.name for d in CORE.defines}

    # Codegen passes the relative brightnesses with the stronger channel at 1.0
    assert _relative(main_cpp, "priority_output") == pytest.approx((0.708, 1.0))
    assert _relative(main_cpp, "rgbww_output") == pytest.approx((1.0, 1.0 / 1.3))
    assert _relative(main_cpp, "disabled_output") == pytest.approx((0.5, 1.0))
    # A ratio of 1.0 is the same as none
    assert _relative(main_cpp, "enabled_output") is None
    assert _relative(main_cpp, "plain_output") is None


def test_priority_code_only_when_used(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("constant_brightness_plain.yaml"))

    assert "USE_LIGHT_COLOR_TEMPERATURE_PRIORITY" not in {d.name for d in CORE.defines}
    assert "USE_LIGHT_WHITE_RELATIVE_BRIGHTNESS" not in {d.name for d in CORE.defines}
