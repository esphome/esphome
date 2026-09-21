"""Config-validation tests for the mt6701 encoder component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.mt6701_i2c import DIRECTION, position12, validate_hysteresis
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        # raw 12-bit counts pass through unchanged
        (0, 0),
        (2048, 2048),
        (4095, 4095),
        # angles are converted to counts
        ("0deg", 0),
        ("45deg", 512),
        ("90°", 1024),
        # a full turn is not representable and must clamp to the max, not wrap to 0
        ("360deg", 4095),
        # percentages of a full revolution
        ("25%", 1024),
        ("50%", 2048),
        ("100%", 4095),
    ],
)
def test_position12_valid(value: int | str, expected: int) -> None:
    assert position12(value) == expected


@pytest.mark.parametrize("value", [-1, 4096, 5000, "400deg", "-10deg"])
def test_position12_rejects_out_of_range(value: int | str) -> None:
    with pytest.raises(cv.Invalid):
        position12(value)


@pytest.mark.parametrize(
    ("lsb", "register_code"),
    [(0, 4), (0.25, 5), (0.5, 6), (1, 0), (2, 1), (4, 2), (8, 3)],
)
def test_hysteresis_maps_lsb_to_register_code(lsb: float, register_code: int) -> None:
    assert validate_hysteresis(lsb) == register_code


@pytest.mark.parametrize("value", [3, 5, 16])
def test_hysteresis_rejects_unsupported_step(value: int) -> None:
    with pytest.raises(cv.Invalid):
        validate_hysteresis(value)


def test_direction_follows_datasheet_semantics() -> None:
    # Datasheet section 8.1: DIR=1 makes the angle increase clockwise.
    assert DIRECTION["CLOCKWISE"] == 1
    assert DIRECTION["COUNTERCLOCKWISE"] == 0


@pytest.mark.parametrize(
    ("config_file", "output_mode_call", "out_pin_call"),
    [
        # ABZ and ANALOG are register code 0
        (
            "mt6701_i2c_modes.yaml",
            "set_output_mode(0)",
            "set_out_pin_mode(0)",
        ),
        # UVW and PWM are register code 1
        (
            "mt6701_i2c_modes_uvw_pwm.yaml",
            "set_output_mode(1)",
            "set_out_pin_mode(1)",
        ),
    ],
)
def test_enum_options_generate_register_codes(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    output_mode_call: str,
    out_pin_call: str,
) -> None:
    """cv.enum returns the key string; codegen must emit its register code.

    Both codes are pinned, so that hardcoding a constant or reading a different
    field is caught.
    """
    main_cpp = generate_main(component_config_path(config_file))

    assert output_mode_call in main_cpp
    assert out_pin_call in main_cpp
