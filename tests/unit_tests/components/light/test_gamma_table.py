"""Tests for the gamma LUT table generation."""

import math

import pytest

from esphome.components.light import MIN_NONZERO_GAMMA_VALUE, generate_gamma_table


def _simulate_gamma_correct_lut(table: list[int], value: float) -> float:
    """Simulate the C++ gamma_correct_lut interpolation from light_state.cpp."""
    if value <= 0.0:
        return 0.0
    if value >= 1.0:
        return 1.0
    scaled = value * 255.0
    idx = int(scaled)
    if idx >= 255:
        return table[255] / 65535.0
    frac = scaled - idx
    a = float(table[idx])
    b = float(table[idx + 1])
    return (a + frac * (b - a)) / 65535.0


def test_table_length() -> None:
    """Table must always have exactly 256 entries."""
    table = generate_gamma_table(2.8)
    assert len(table) == 256


def test_index_zero_is_zero() -> None:
    """Index 0 must be 0 so true off remains off."""
    for gamma in (1.0, 2.0, 2.2, 2.8, 3.0):
        table = generate_gamma_table(gamma)
        assert table[0] == 0, f"gamma={gamma}"


def test_index_255_is_max() -> None:
    """Index 255 must be 65535 (full on)."""
    for gamma in (1.0, 2.0, 2.2, 2.8, 3.0):
        table = generate_gamma_table(gamma)
        assert table[255] == 65535, f"gamma={gamma}"


@pytest.mark.parametrize("gamma", [1.0, 2.0, 2.2, 2.8, 3.0])
def test_nonzero_indices_are_nonzero(gamma: float) -> None:
    """All indices > 0 must produce non-zero values.

    This prevents zero_means_zero breakage: non-zero input must always
    produce non-zero output so FloatOutput applies min_power scaling.
    """
    table = generate_gamma_table(gamma)
    for i in range(1, 256):
        assert table[i] >= 1, f"gamma={gamma}, index {i}: got {table[i]}"


@pytest.mark.parametrize("gamma", [1.0, 2.0, 2.2, 2.8, 3.0])
def test_table_monotonically_nondecreasing(gamma: float) -> None:
    """The gamma table must be monotonically non-decreasing."""
    table = generate_gamma_table(gamma)
    for i in range(1, 256):
        assert table[i] >= table[i - 1], (
            f"gamma={gamma}: table[{i}]={table[i]} < table[{i - 1}]={table[i - 1]}"
        )


def test_linear_gamma() -> None:
    """With gamma=0 (linear), table should be evenly spaced."""
    table = generate_gamma_table(0)
    assert table[0] == 0
    assert table[128] == round(128 / 255.0 * 65535)
    assert table[255] == 65535


@pytest.mark.parametrize("brightness", [0.01, 0.005, 0.001, 1 / 255])
def test_small_brightness_nonzero_after_lut(brightness: float) -> None:
    """Small but non-zero brightness must produce non-zero output through the LUT.

    Regression test for #15055: with zero_means_zero=true, a gamma-corrected
    value of exactly 0.0 causes FloatOutput to skip min_power scaling, turning
    the LED off instead of to minimum brightness.
    """
    table = generate_gamma_table(2.8)
    result = _simulate_gamma_correct_lut(table, brightness)
    assert result > 0.0, (
        f"brightness={brightness}: gamma LUT returned 0.0, would break zero_means_zero"
    )


@pytest.mark.parametrize("gamma", [1.0, 2.0, 2.2, 2.8, 3.0])
def test_small_brightness_nonzero_all_gammas(gamma: float) -> None:
    """1% brightness must be non-zero for all common gamma values."""
    table = generate_gamma_table(gamma)
    result = _simulate_gamma_correct_lut(table, 0.01)
    assert result > 0.0, f"gamma={gamma}: 1% brightness returned 0.0"


def test_lut_zero_returns_zero() -> None:
    """LUT with input 0.0 must return 0.0."""
    table = generate_gamma_table(2.8)
    assert _simulate_gamma_correct_lut(table, 0.0) == 0.0


def test_lut_one_returns_one() -> None:
    """LUT with input 1.0 must return 1.0."""
    table = generate_gamma_table(2.8)
    assert _simulate_gamma_correct_lut(table, 1.0) == 1.0


def test_lut_output_monotonically_nondecreasing() -> None:
    """LUT output must be monotonically non-decreasing across the full range."""
    table = generate_gamma_table(2.8)
    prev = 0.0
    for i in range(1001):
        value = i / 1000.0
        result = _simulate_gamma_correct_lut(table, value)
        assert result >= prev, f"value={value}: result {result} < previous {prev}"
        prev = result


def _dead_zone_breakpoint(gamma: float) -> int:
    """Smallest code whose power-curve value survives (value + 128) / 257 as non-zero."""
    return math.ceil(255 * (1 / 510.0) ** (1 / gamma))


def _to_8_bit(value: int) -> int:
    """Simulate ESPColorCorrection::gamma_correct_'s (value + 128) / 257 conversion."""
    return (value + 128) // 257


@pytest.mark.parametrize("gamma", [1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0])
def test_nonzero_indices_survive_16_to_8_bit_conversion(gamma: float) -> None:
    """Regression test for esphome/esphome#18842: low codes must no longer round to 0."""
    table = generate_gamma_table(gamma)
    for i in range(1, 256):
        assert _to_8_bit(table[i]) >= 1, (
            f"gamma={gamma}, index {i}: table value {table[i]} collapses to 0 "
            "after (value + 128) / 257"
        )


def test_dead_zone_fixed_at_gamma_2_8() -> None:
    """Reproduce the reporter's own numbers from esphome/esphome#18842 at gamma=2.8."""
    gamma = 2.8
    n0 = _dead_zone_breakpoint(gamma)
    assert n0 == 28  # matches the "brightness 28 and above works normally" report

    table = generate_gamma_table(gamma)
    for i in range(1, n0):
        assert _to_8_bit(table[i]) >= 1, f"index {i} still collapses to 0"

    for i in range(n0, 256):
        raw_power_value = min(65535, round((i / 255.0) ** gamma * 65535))
        assert table[i] == raw_power_value, (
            f"index {i}: table[{i}]={table[i]} differs from the untouched power "
            f"curve value {raw_power_value}"
        )


@pytest.mark.parametrize("gamma", [1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0])
def test_power_curve_untouched_above_dead_zone(gamma: float) -> None:
    """Above the dead zone, table values must be bit-identical to the raw power curve."""
    n0 = _dead_zone_breakpoint(gamma)
    table = generate_gamma_table(gamma)
    for i in range(n0, 256):
        raw_power_value = min(65535, round((i / 255.0) ** gamma * 65535))
        assert table[i] == raw_power_value, f"gamma={gamma}, index {i}"


@pytest.mark.parametrize("gamma", [1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0])
def test_dead_zone_values_are_min_nonzero_gamma_value(gamma: float) -> None:
    """Within the dead zone, table values must be clamped to MIN_NONZERO_GAMMA_VALUE."""
    n0 = _dead_zone_breakpoint(gamma)
    table = generate_gamma_table(gamma)
    for i in range(1, n0):
        assert table[i] == MIN_NONZERO_GAMMA_VALUE, f"gamma={gamma}, index {i}"


@pytest.mark.parametrize("gamma", [1.0, 1.8, 2.0, 2.2, 2.8, 3.0, 4.0])
def test_reverse_search_precondition_nondecreasing(gamma: float) -> None:
    """gamma_table_reverse_search needs a non-decreasing table; the fix must preserve that."""
    table = generate_gamma_table(gamma)
    for i in range(1, 256):
        assert table[i] >= table[i - 1], f"gamma={gamma}: table[{i}] < table[{i - 1}]"
