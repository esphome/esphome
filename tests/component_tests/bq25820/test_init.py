"""Tests for the bq25820 configuration: current limits, register packing, watchdog and NTC."""

from collections.abc import Callable
import math
from pathlib import Path

import pytest

from esphome.components.bq25820 import (
    CONF_AC_SNS_RES,
    CONF_BAT_SNS_RES,
    WATCHDOG_CHOICES,
    _check_watchdog,
    _current_code,
    _validate_current_limits,
    _watchdog_boolean,
    binary_sensor,
    sensor,
    text_sensor,
    validate_sense_resistance,
)
from esphome.components.bq25820.sensor import (
    CONF_RESISTOR,
    ntc_calibration,
    validate_reference_temperature,
)
from esphome.components.const import (
    CONF_B_CONSTANT,
    CONF_CHARGE_CURRENT,
    CONF_INPUT_CURRENT_LIMIT,
    CONF_PRECHARGE_CURRENT,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_REFERENCE_RESISTANCE,
    CONF_REFERENCE_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    CONF_WATCHDOG,
)
from esphome.types import ConfigType


def test_current_code_scales_with_shunt() -> None:
    # 50 mA per step with the 5 mOhm reference shunt, 125 mA per step at 2 mOhm
    assert _current_code(1.0, 5.0) == 20
    assert _current_code(1.0, 2.0) == 8
    # A 2 mOhm battery shunt needs 2.5 times fewer steps for the same current
    assert _current_code(3.0, 2.0) == 24


def test_charge_current_below_range_is_rejected() -> None:
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0, CONF_CHARGE_CURRENT: 0.1}
    with pytest.raises(cv.Invalid, match="0.4-20 A with a 5 mΩ bat_sns_res"):
        _validate_current_limits(config)


@pytest.mark.parametrize(
    ("key", "current", "valid"),
    [
        (CONF_CHARGE_CURRENT, 0.39, False),  # rounds to the 0.4 A code
        (CONF_CHARGE_CURRENT, 0.4, True),
        (CONF_CHARGE_CURRENT, 20.0, True),
        (CONF_CHARGE_CURRENT, 20.01, False),
        (CONF_PRECHARGE_CURRENT, 0.24, False),  # rounds to the 0.25 A code
        (CONF_PRECHARGE_CURRENT, 0.25, True),
        (CONF_PRECHARGE_CURRENT, 10.0, True),
    ],
)
def test_current_near_the_range_ends(key: str, current: float, valid: bool) -> None:
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0, key: current}
    if valid:
        assert _validate_current_limits(config) is config
    else:
        with pytest.raises(cv.Invalid):
            _validate_current_limits(config)


def test_input_current_range_follows_shunt() -> None:
    # 50 A fits the 2 mOhm reference shunt but not a 4 mOhm one
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0, CONF_INPUT_CURRENT_LIMIT: 50}
    assert _validate_current_limits(config) is config
    config[CONF_AC_SNS_RES] = 4.0
    with pytest.raises(cv.Invalid, match="0.5-25 A"):
        _validate_current_limits(config)


def test_unset_limits_are_skipped() -> None:
    config = {CONF_BAT_SNS_RES: 5.0, CONF_AC_SNS_RES: 2.0}
    assert _validate_current_limits(config) is config


def _hub_config(main_cpp: str, hub: str) -> str:
    table = main_cpp.split(f"new({hub}) bq25820::BQ25820Component(", 1)[1].split(
        ",", 1
    )[0]
    head = f"static constexpr bq25820::BQ25820Config {table}[] PROGMEM = {{"
    return main_cpp.split(head, 1)[1].split("};\n", 1)[0]


def test_config_is_packed_into_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Register values per the datasheet fields; tests/components/bq25820 runs the same values."""
    main_cpp = generate_main(component_config_path("packed_config.yaml"))
    assert _hub_config(main_cpp, "charger_defaults") == (
        "bq25820::BQ25820Config{\n"
        "  .limits = {0, 0, 0, 880, 1000},\n"
        "  .controls = {0x10, 0x0A, 0x00, 0x00, 0x16, 0x01, 0xC9},\n"
        "  .ibat_scale = 0.002f,\n"
        "  .iac_scale = 0.002f,\n"
        "}"
    )
    assert _hub_config(main_cpp, "charger_set") == (
        "bq25820::BQ25820Config{\n"
        "  .limits = {240, 80, 256, 8000, 660},\n"
        "  .controls = {0x17, 0x9C, 0x04, 0x03, 0xFF, 0x00, 0xCC},\n"
        "  .ibat_scale = 0.002f,\n"
        "  .iac_scale = 0.002f,\n"
        "}"
    )
    # force_mppt_sweep stays out of the table: it is written once, not after a watchdog reset
    assert "BQ25820Component(bq25820_config, false);" in main_cpp
    assert "BQ25820Component(bq25820_config_2, true);" in main_cpp
    # The default NTC: 1/T0 and 1/B; the custom one moves a by ln(47k / 100k) / B
    a, b = ntc_calibration(
        {
            CONF_B_CONSTANT: 3950,
            CONF_REFERENCE_RESISTANCE: 100000,
            CONF_REFERENCE_TEMPERATURE: 30,
            CONF_RESISTOR: 47000,
        }
    )
    assert a == pytest.approx(1 / 303.15 + math.log(0.47) / 3950)
    assert b == pytest.approx(1 / 3950)
    assert (
        f"charger_defaults->set_ntc_calibration({1 / 298.15!r}f, {1 / 3435!r}f);"
        in main_cpp
    )
    assert f"charger_set->set_ntc_calibration({a!r}f, {b!r}f);" in main_cpp


def _watchdog_config(interval: str, watchdog: object) -> ConfigType:
    return {
        CONF_WATCHDOG: cv.enum(WATCHDOG_CHOICES, lower=True)(
            _watchdog_boolean(watchdog)
        ),
        CONF_UPDATE_INTERVAL: cv.update_interval(interval),
    }


@pytest.mark.parametrize(
    "interval, watchdog",
    [
        ("60s", "disabled"),
        ("60s", False),
        ("60s", True),
        ("60s", "80s"),
        ("60s", "160s"),
        ("30s", "40s"),
    ],
)
def test_update_interval_within_watchdog_passes(
    interval: str, watchdog: object
) -> None:
    _check_watchdog(_watchdog_config(interval, watchdog))


@pytest.mark.parametrize(
    "interval, watchdog, period",
    [
        ("60s", "40s", 40),
        ("40s", "40s", 40),
        ("90s", "80s", 80),
        ("5min", True, 160),
    ],
)
def test_update_interval_past_watchdog_is_rejected(
    interval: str, watchdog: object, period: int
) -> None:
    with pytest.raises(
        cv.Invalid, match=f"shorter than the {period} s watchdog"
    ) as err:
        _check_watchdog(_watchdog_config(interval, watchdog))
    assert err.value.path == [CONF_UPDATE_INTERVAL]


@pytest.mark.parametrize("watchdog", ["disabled", "160s"])
def test_update_interval_never_is_rejected(watchdog: str) -> None:
    """The poll restores the configuration after a charger reset, so it must run."""
    with pytest.raises(cv.Invalid, match="cannot be never") as err:
        _check_watchdog(_watchdog_config("never", watchdog))
    assert err.value.path == [CONF_UPDATE_INTERVAL]


def test_watchdog_boolean_maps_to_a_period() -> None:
    assert _watchdog_boolean(True) == "160s"
    assert _watchdog_boolean(False) == "disabled"
    assert _watchdog_boolean("40s") == "40s"


@pytest.mark.parametrize(
    "schema",
    [binary_sensor.CONFIG_SCHEMA, sensor.CONFIG_SCHEMA, text_sensor.CONFIG_SCHEMA],
)
def test_platform_needs_an_entity(schema: cv.Schema) -> None:
    with pytest.raises(cv.Invalid, match="at least one of"):
        schema({})


@pytest.mark.parametrize("value", ["-273.15°C", "-300°C", "0K"])
def test_reference_temperature_at_or_below_absolute_zero_is_rejected(
    value: str,
) -> None:
    with pytest.raises(cv.Invalid):
        validate_reference_temperature(value)


def test_reference_temperature_above_absolute_zero_passes() -> None:
    assert validate_reference_temperature("-273°C") == pytest.approx(-273)
    assert validate_reference_temperature("25°C") == pytest.approx(25)


@pytest.mark.parametrize(
    ("value", "milliohms"), [("5mΩ", 5.0), ("0.01mΩ", 0.01), ("10mΩ", 10.0)]
)
def test_sense_resistance_in_range_passes(value: str, milliohms: float) -> None:
    assert validate_sense_resistance(value) == pytest.approx(milliohms)


@pytest.mark.parametrize("value", ["5Ω", "0.005mΩ", "11mΩ"])
def test_sense_resistance_out_of_range_is_rejected(value: str) -> None:
    with pytest.raises(cv.Invalid, match="Sense resistance must be 0.01-10 mΩ"):
        validate_sense_resistance(value)
