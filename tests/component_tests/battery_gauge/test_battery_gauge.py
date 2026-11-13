"""Tests for the battery_gauge hub configuration schema."""

from __future__ import annotations

import itertools
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.battery_gauge import (
    CELL_FULL_CHARGE_VOLTAGE,
    CHEMISTRY_CUSTOM,
    CHEMISTRY_LEAD_ACID_AGM,
    CHEMISTRY_LEAD_ACID_FLOODED,
    CHEMISTRY_LIFEPO4,
    CONF_ACCEPTANCE_KNEE,
    CONF_CAPACITY_RATE,
    CONF_CELL_COUNT,
    CONF_CHEMISTRY,
    CONF_CURRENT_SOURCE,
    CONF_EFFICIENCY,
    CONF_FULL_CHARGE_DWELL,
    CONF_MAX_CHARGE_VOLTAGE,
    CONF_PEUKERT_EXPONENT,
    CONF_TAIL_CURRENT,
    CONF_VOLTAGE_SOURCE,
    CONFIG_SCHEMA,
    capacity_ah,
)
from esphome.const import CONF_CAPACITY, CONF_INITIAL_STATE

# A unique counter so that every generated config has a distinct id.
_counter = itertools.count()


def _config(**overrides: Any) -> dict[str, Any]:
    """Build a minimal valid battery_gauge hub config with a unique id."""
    n = next(_counter)
    config: dict[str, Any] = {
        "id": f"bg_{n}",
        CONF_VOLTAGE_SOURCE: "voltage_sensor",
        CONF_CURRENT_SOURCE: "current_sensor",
        CONF_CAPACITY: "5Ah",
        CONF_MAX_CHARGE_VOLTAGE: "4.2V",
        CONF_CHEMISTRY: CHEMISTRY_LIFEPO4,
    }
    config.update(overrides)
    return config


# --- minimal / full config ---


def test_minimal_config_is_valid() -> None:
    """A config with only the required keys validates and applies defaults."""
    result = CONFIG_SCHEMA(_config())

    assert result[CONF_CAPACITY] == 5.0
    assert result[CONF_MAX_CHARGE_VOLTAGE] == 4.2
    # default efficiency
    assert result[CONF_EFFICIENCY] == 0.98
    # initial_state is optional and has no default
    assert CONF_INITIAL_STATE not in result


def test_full_config_is_valid() -> None:
    """All optional keys are carried through validation."""
    result = CONFIG_SCHEMA(
        _config(
            **{
                CONF_EFFICIENCY: 0.9,
                CONF_INITIAL_STATE: 0.5,
            }
        )
    )

    assert result[CONF_EFFICIENCY] == 0.9
    assert result[CONF_INITIAL_STATE] == 0.5


# --- required keys ---


@pytest.mark.parametrize(
    "missing_key",
    [
        CONF_VOLTAGE_SOURCE,
        CONF_CURRENT_SOURCE,
        CONF_CAPACITY,
        CONF_CHEMISTRY,
    ],
)
def test_required_key_missing_raises(missing_key: str) -> None:
    """Omitting any required key is a validation error."""
    config = _config()
    del config[missing_key]
    with pytest.raises(cv.Invalid, match=rf"required key not provided.*{missing_key}"):
        CONFIG_SCHEMA(config)


# --- max_charge_voltage / cell_count ---


def test_max_charge_voltage_and_cell_count_both_missing_raises() -> None:
    """Neither max_charge_voltage nor cell_count given is a validation error."""
    config = _config()
    del config[CONF_MAX_CHARGE_VOLTAGE]
    with pytest.raises(cv.Invalid, match="exactly one of"):
        CONFIG_SCHEMA(config)


def test_max_charge_voltage_and_cell_count_both_given_raises() -> None:
    """Giving both max_charge_voltage and cell_count is a validation error."""
    with pytest.raises(cv.Invalid, match="more than one of"):
        CONFIG_SCHEMA(_config(**{CONF_CELL_COUNT: 4}))


@pytest.mark.parametrize(
    "chemistry",
    [CHEMISTRY_LIFEPO4, CHEMISTRY_LEAD_ACID_FLOODED, CHEMISTRY_LEAD_ACID_AGM],
)
def test_cell_count_derives_max_charge_voltage(chemistry: str) -> None:
    """cell_count is resolved to max_charge_voltage using the chemistry's per-cell voltage."""
    config = _config(**{CONF_CHEMISTRY: chemistry, CONF_CELL_COUNT: 4})
    del config[CONF_MAX_CHARGE_VOLTAGE]
    result = CONFIG_SCHEMA(config)

    assert CONF_CELL_COUNT not in result
    assert result[CONF_MAX_CHARGE_VOLTAGE] == pytest.approx(
        4 * CELL_FULL_CHARGE_VOLTAGE[chemistry]
    )


def test_cell_count_with_custom_chemistry_raises() -> None:
    """The custom chemistry has no standard per-cell voltage, so cell_count is rejected."""
    config = _config(
        **{
            CONF_CHEMISTRY: CHEMISTRY_CUSTOM,
            CONF_CELL_COUNT: 4,
            CONF_PEUKERT_EXPONENT: 1.2,
            CONF_CAPACITY_RATE: "10h",
        }
    )
    del config[CONF_MAX_CHARGE_VOLTAGE]
    with pytest.raises(cv.Invalid, match="cannot be used with chemistry 'custom'"):
        CONFIG_SCHEMA(config)


# --- capacity parsing ---


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("5Ah", 5.0),
        ("2.5ah", 2.5),
        ("10AH", 10.0),
        ("3aH", 3.0),
        ("7", 7.0),
        (4.2, 4.2),
    ],
)
def test_capacity_units(value: Any, expected: float) -> None:
    """capacity_ah accepts an optional Ah suffix in any case."""
    assert capacity_ah(value) == expected


def test_capacity_bad_suffix_raises() -> None:
    """An unrecognised capacity suffix is rejected."""
    with pytest.raises(cv.Invalid, match="capacity suffix"):
        capacity_ah("5xyz")


# --- initial_state range ---


@pytest.mark.parametrize("value", [0.0, 0.5, 1.0, "50%"])
def test_initial_state_valid(value: Any) -> None:
    """initial_state accepts the inclusive range 0..1 (and percent strings)."""
    result = CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: value}))
    assert 0.0 <= result[CONF_INITIAL_STATE] <= 1.0


def test_initial_state_above_one_raises() -> None:
    """A bare number above 1.0 requires a percent sign and is rejected."""
    with pytest.raises(cv.Invalid, match="percent sign"):
        CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: 1.5}))


def test_initial_state_negative_raises() -> None:
    """initial_state below 0 is out of range."""
    with pytest.raises(cv.Invalid, match="at least 0"):
        CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: -0.1}))


# --- chemistry selection and defaulting ---


def test_chemistry_is_required() -> None:
    """Chemistry has no default: every config must pick one explicitly."""
    config = _config()
    del config[CONF_CHEMISTRY]
    with pytest.raises(
        cv.Invalid, match=rf"required key not provided.*{CONF_CHEMISTRY}"
    ):
        CONFIG_SCHEMA(config)


def test_chemistry_lifepo4_has_no_lead_acid_keys() -> None:
    """Choosing lifepo4 doesn't pull in any lead-acid-only keys."""
    result = CONFIG_SCHEMA(_config())

    assert result[CONF_CHEMISTRY] == CHEMISTRY_LIFEPO4
    assert CONF_ACCEPTANCE_KNEE not in result
    assert CONF_PEUKERT_EXPONENT not in result
    assert CONF_CAPACITY_RATE not in result
    assert CONF_TAIL_CURRENT not in result
    assert CONF_FULL_CHARGE_DWELL not in result


@pytest.mark.parametrize(
    "lead_acid_key",
    [
        CONF_ACCEPTANCE_KNEE,
        CONF_PEUKERT_EXPONENT,
        CONF_CAPACITY_RATE,
        CONF_TAIL_CURRENT,
        CONF_FULL_CHARGE_DWELL,
    ],
)
def test_lead_acid_key_with_lifepo4_raises(lead_acid_key: str) -> None:
    """Any lead-acid-only key is rejected when chemistry is lifepo4."""
    dummy_values = {
        CONF_ACCEPTANCE_KNEE: "50%",
        CONF_PEUKERT_EXPONENT: 1.2,
        CONF_CAPACITY_RATE: "1h",
        CONF_TAIL_CURRENT: "50%",
        CONF_FULL_CHARGE_DWELL: "1h",
    }
    overrides = {lead_acid_key: dummy_values[lead_acid_key]}
    with pytest.raises(cv.Invalid, match="only valid when"):
        CONFIG_SCHEMA(_config(**overrides))


def test_chemistry_lead_acid_flooded_applies_preset() -> None:
    """Choosing lead_acid_flooded fills in its preset defaults."""
    result = CONFIG_SCHEMA(_config(**{CONF_CHEMISTRY: CHEMISTRY_LEAD_ACID_FLOODED}))

    assert result[CONF_ACCEPTANCE_KNEE] == pytest.approx(0.80)
    assert result[CONF_PEUKERT_EXPONENT] == pytest.approx(1.25)
    assert result[CONF_CAPACITY_RATE].total_hours == 20
    assert result[CONF_TAIL_CURRENT] == pytest.approx(0.04)
    assert result[CONF_FULL_CHARGE_DWELL].total_minutes == 3


def test_chemistry_lead_acid_agm_applies_preset() -> None:
    """Choosing lead_acid_agm fills in its own preset defaults."""
    result = CONFIG_SCHEMA(_config(**{CONF_CHEMISTRY: CHEMISTRY_LEAD_ACID_AGM}))

    assert result[CONF_PEUKERT_EXPONENT] == pytest.approx(1.15)


def test_chemistry_preset_can_be_overridden() -> None:
    """An explicit key wins over the chemistry preset's default."""
    result = CONFIG_SCHEMA(
        _config(
            **{
                CONF_CHEMISTRY: CHEMISTRY_LEAD_ACID_FLOODED,
                CONF_TAIL_CURRENT: "10%",
            }
        )
    )

    assert result[CONF_TAIL_CURRENT] == pytest.approx(0.10)
    # Untouched keys still get the preset's value.
    assert result[CONF_ACCEPTANCE_KNEE] == pytest.approx(0.80)


def test_chemistry_custom_defaults_to_no_op_values() -> None:
    """With no overrides, "custom" behaves identically to lifepo4 (no-op values)."""
    result = CONFIG_SCHEMA(_config(**{CONF_CHEMISTRY: CHEMISTRY_CUSTOM}))

    assert result[CONF_ACCEPTANCE_KNEE] == pytest.approx(1.0)
    assert result[CONF_PEUKERT_EXPONENT] == pytest.approx(1.0)
    assert result[CONF_TAIL_CURRENT] == pytest.approx(0.02)
    assert result[CONF_FULL_CHARGE_DWELL].total_seconds == 0


@pytest.mark.parametrize(
    "overrides",
    [
        {CONF_PEUKERT_EXPONENT: 1.2},
        {CONF_CAPACITY_RATE: "10h"},
    ],
)
def test_chemistry_custom_peukert_and_rate_must_be_paired(
    overrides: dict[str, Any],
) -> None:
    """The "custom" chemistry has no preset to complete a partial pair from: both or neither."""
    with pytest.raises(cv.Invalid, match="must be specified together"):
        CONFIG_SCHEMA(_config(**{CONF_CHEMISTRY: CHEMISTRY_CUSTOM, **overrides}))


def test_chemistry_custom_peukert_and_rate_together_is_valid() -> None:
    """Supplying both keys together for "custom" is accepted."""
    result = CONFIG_SCHEMA(
        _config(
            **{
                CONF_CHEMISTRY: CHEMISTRY_CUSTOM,
                CONF_PEUKERT_EXPONENT: 1.2,
                CONF_CAPACITY_RATE: "10h",
            }
        )
    )

    assert result[CONF_PEUKERT_EXPONENT] == pytest.approx(1.2)
    assert result[CONF_CAPACITY_RATE].total_hours == 10
