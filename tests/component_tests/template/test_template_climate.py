"""Tests for template climate config validation."""

from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.template.climate import (
    CONF_SET_TARGET_HUMIDITY_ACTION,
    CONF_SET_TARGET_TEMPERATURE_ACTION,
    CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION,
    CONF_SET_TARGET_TEMPERATURE_LOW_ACTION,
    CONF_SUPPORTS_CURRENT_HUMIDITY,
    CONF_SUPPORTS_CURRENT_TEMPERATURE,
    CONF_SUPPORTS_TARGET_HUMIDITY,
    CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE,
    CONF_TARGET_HUMIDITY,
    _resolve_supports,
    _sensor_reports_in_the_entity_unit,
    _validate_initial_state,
    _validate_set_actions,
)
from esphome.config import Config, read_config
from esphome.const import (
    CONF_HUMIDITY_SENSOR,
    CONF_ID,
    CONF_INITIAL_STATE,
    CONF_SENSOR,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_LOW,
    CONF_UNIT_OF_MEASUREMENT,
    UNIT_FAHRENHEIT,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType


def test_supports_current_temperature_derived_from_sensor() -> None:
    config: ConfigType = {CONF_SENSOR: "some_sensor"}
    assert _resolve_supports(config)[CONF_SUPPORTS_CURRENT_TEMPERATURE] is True


def test_supports_current_temperature_false_without_sensor() -> None:
    assert _resolve_supports({})[CONF_SUPPORTS_CURRENT_TEMPERATURE] is False


def test_supports_current_temperature_explicit_true_without_sensor_allowed() -> None:
    # The value can still be reported with climate.template.publish.
    config: ConfigType = {CONF_SUPPORTS_CURRENT_TEMPERATURE: True}
    assert _resolve_supports(config)[CONF_SUPPORTS_CURRENT_TEMPERATURE] is True


def test_supports_current_temperature_false_with_sensor_rejected() -> None:
    config: ConfigType = {
        CONF_SENSOR: "some_sensor",
        CONF_SUPPORTS_CURRENT_TEMPERATURE: False,
    }
    with pytest.raises(cv.Invalid, match="cannot be false"):
        _resolve_supports(config)


def test_supports_current_humidity_false_with_sensor_rejected() -> None:
    config: ConfigType = {
        CONF_HUMIDITY_SENSOR: "some_sensor",
        CONF_SUPPORTS_CURRENT_HUMIDITY: False,
    }
    with pytest.raises(cv.Invalid, match="cannot be false"):
        _resolve_supports(config)


def test_two_point_derived_from_set_actions() -> None:
    config: ConfigType = {
        CONF_SET_TARGET_TEMPERATURE_LOW_ACTION: [{}],
        CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION: [{}],
    }
    assert _resolve_supports(config)[CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE] is True


def test_two_point_false_with_set_action_rejected() -> None:
    config: ConfigType = {
        CONF_SET_TARGET_TEMPERATURE_LOW_ACTION: [{}],
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: False,
    }
    with pytest.raises(cv.Invalid, match="cannot be false"):
        _resolve_supports(config)


def test_target_humidity_derived_from_set_action() -> None:
    config: ConfigType = {CONF_SET_TARGET_HUMIDITY_ACTION: [{}]}
    assert _resolve_supports(config)[CONF_SUPPORTS_TARGET_HUMIDITY] is True


def test_set_target_temperature_low_requires_high() -> None:
    config: ConfigType = {CONF_SET_TARGET_TEMPERATURE_LOW_ACTION: [{}]}
    with pytest.raises(cv.Invalid, match="must be used together"):
        _validate_set_actions(config)


def test_set_target_temperature_conflicts_with_two_point_actions() -> None:
    config: ConfigType = {
        CONF_SET_TARGET_TEMPERATURE_ACTION: [{}],
        CONF_SET_TARGET_TEMPERATURE_LOW_ACTION: [{}],
        CONF_SET_TARGET_TEMPERATURE_HIGH_ACTION: [{}],
    }
    with pytest.raises(cv.Invalid, match="cannot be used together"):
        _validate_set_actions(config)


def test_initial_state_target_temperature_rejected_with_two_point() -> None:
    config: ConfigType = {
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: True,
        CONF_SUPPORTS_TARGET_HUMIDITY: False,
        CONF_INITIAL_STATE: {CONF_TARGET_TEMPERATURE: 21.0},
    }
    with pytest.raises(cv.Invalid, match="is not available"):
        _validate_initial_state(config)


def test_initial_state_two_point_values_rejected_without_two_point() -> None:
    config: ConfigType = {
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: False,
        CONF_SUPPORTS_TARGET_HUMIDITY: False,
        CONF_INITIAL_STATE: {
            CONF_TARGET_TEMPERATURE_LOW: 18.0,
            CONF_TARGET_TEMPERATURE_HIGH: 24.0,
        },
    }
    with pytest.raises(cv.Invalid, match="requires"):
        _validate_initial_state(config)


def test_initial_state_target_humidity_rejected_without_support() -> None:
    config: ConfigType = {
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: False,
        CONF_SUPPORTS_TARGET_HUMIDITY: False,
        CONF_INITIAL_STATE: {CONF_TARGET_HUMIDITY: 50},
    }
    with pytest.raises(cv.Invalid, match="requires"):
        _validate_initial_state(config)


def test_initial_state_matching_two_point_accepted() -> None:
    config: ConfigType = {
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: True,
        CONF_SUPPORTS_TARGET_HUMIDITY: True,
        CONF_INITIAL_STATE: {
            CONF_TARGET_TEMPERATURE_LOW: 18.0,
            CONF_TARGET_TEMPERATURE_HIGH: 24.0,
            CONF_TARGET_HUMIDITY: 50,
        },
    }
    assert _validate_initial_state(config) is config


def test_initial_state_temperature_follows_unit_of_measurement() -> None:
    config: ConfigType = {
        CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT),
        CONF_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE: False,
        CONF_SUPPORTS_TARGET_HUMIDITY: False,
        CONF_INITIAL_STATE: {CONF_TARGET_TEMPERATURE: (21.0, "°C")},
    }
    _validate_initial_state(config)
    assert config[CONF_INITIAL_STATE][CONF_TARGET_TEMPERATURE] == pytest.approx(69.8)


def _config_with_sensor(unit: str | None) -> Config:
    temp = ID("temp", is_declaration=True, type="sensor::Sensor")
    full = Config()
    full["sensor"] = [
        {CONF_ID: temp}
        if unit is None
        else {CONF_ID: temp, CONF_UNIT_OF_MEASUREMENT: unit}
    ]
    full.declare_ids.append((temp, ["sensor", 0, CONF_ID]))
    return full


@pytest.mark.parametrize(
    ("sensor_unit", "accepted"),
    [
        ("°C", False),
        ("°F", True),
        ("%", True),
        (None, True),
    ],
    ids=["celsius", "fahrenheit", "not a temperature", "no unit"],
)
def test_a_temperature_sensor_must_report_in_the_climate_unit(
    sensor_unit: str | None, accepted: bool
) -> None:
    """The sensor state is copied as is, so a temperature sensor in another unit is rejected."""
    config: ConfigType = {
        CONF_SENSOR: ID("temp", is_declaration=False),
        CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT),
    }
    token = fv.full_config.set(_config_with_sensor(sensor_unit))
    try:
        if accepted:
            assert _sensor_reports_in_the_entity_unit(config) is config
        else:
            with pytest.raises(
                cv.Invalid, match="reports in °C but the climate reports in °F"
            ):
                _sensor_reports_in_the_entity_unit(config)
    finally:
        fv.full_config.reset(token)


def test_a_climate_without_a_sensor_has_no_unit_to_check() -> None:
    config: ConfigType = {
        CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT)
    }
    assert _sensor_reports_in_the_entity_unit(config) is config


_SENSORS = """
sensor:
  - platform: template
    id: source_temp
    unit_of_measurement: "{source_unit}"
  - platform: copy
    id: copied_temp
    source_id: source_temp
  - platform: copy
    id: copy_of_copy
    source_id: copied_temp
"""

_CLIMATE = """
climate:
  - platform: template
    id: thermostat
    name: Thermostat
    unit_of_measurement: °F
    sensor: {sensor}
    supported_modes: [HEAT]
"""


@pytest.mark.parametrize(
    "climate_first", [True, False], ids=["climate first", "sensor first"]
)
@pytest.mark.parametrize(
    ("source_unit", "sensor", "accepted"),
    [
        ("°C", "copied_temp", False),
        ("°F", "copied_temp", True),
        ("°C", "copy_of_copy", False),
        ("°F", "copy_of_copy", True),
    ],
    ids=[
        "copy celsius",
        "copy fahrenheit",
        "copy of copy celsius",
        "copy of copy fahrenheit",
    ],
)
def test_an_inherited_sensor_unit_is_checked_in_any_section_order(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    climate_first: bool,
    source_unit: str,
    sensor: str,
    accepted: bool,
) -> None:
    """A copy sensor takes its unit from its source during final validation, which runs per
    component in config order; the climate check must see that unit before or after it."""
    sections = [
        _SENSORS.format(source_unit=source_unit),
        _CLIMATE.format(sensor=sensor),
    ]
    if climate_first:
        sections.reverse()
    config_path = tmp_path / "test.yaml"
    config_path.write_text(
        "esphome:\n  name: test\n\nesp8266:\n  board: esp01_1m\n" + "".join(sections)
    )
    CORE.config_path = config_path
    result = read_config({})
    if accepted:
        assert result is not None
    else:
        assert result is None
        assert "reports in °C but the climate reports in °F" in capsys.readouterr().out
