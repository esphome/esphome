"""Exercise TMP102 sensor validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import tmp102 as component
from esphome.components.tmp102.sensor import CONFIG_SCHEMA
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


def test_basic_unchanged(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("basic.yaml"))
    assert "set_configuration" not in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_" not in defines


def test_advanced_options_enable_configuration_once(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("advanced.yaml"))
    assert source.count("set_configuration(") == 1
    assert "set_configuration(3648, 7680, 6400)" in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_CONFIGURE" in defines


def test_alert_entity_is_read_only(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("alert.yaml"))
    assert "set_alert_binary_sensor" in source
    assert "set_configuration" not in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_BINARY_SENSOR" in defines
    assert "USE_TMP102_CONFIGURE" not in defines


def test_duplicate_alert_entities_are_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("duplicate_alert.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert (
        "Only one TMP102 binary_sensor may reference each temperature sensor"
        in captured.out
    )


def test_status_entity_is_read_only(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("status.yaml"))
    assert "set_threshold_status_text_sensor" in source
    assert "set_configuration" not in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_TEXT_SENSOR" in defines
    assert "USE_TMP102_CONFIGURE" not in defines


def test_duplicate_status_entities_are_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("duplicate_status.yaml")
    assert read_config({}) is None
    assert (
        "Only one TMP102 text_sensor may reference each temperature sensor"
        in capsys.readouterr().out
    )


def test_threshold_numbers_enable_configuration_once(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("numbers.yaml"))
    assert source.count("set_configuration(") == 1
    assert "set_configuration(128, 20480, 19200)" in source
    assert source.count("set_limit_control(") == 2
    assert "tmp102::TMP102_LIMIT_HIGH" in source
    assert "tmp102::TMP102_LIMIT_LOW" in source
    assert "set_restore_value(false)" in source
    assert "set_max_value(127.9375f)" in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_NUMBER" in defines
    assert "USE_TMP102_CONFIGURE" in defines


def test_threshold_numbers_use_sensor_thresholds(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("numbers_inherit.yaml"))
    assert source.count("set_configuration(") == 1
    assert "set_configuration(144, 17920, 15360)" in source
    assert "set_max_value(150.0f)" in source


def test_threshold_number_initial_value_is_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("numbers_initial_value.yaml")
    assert read_config({}) is None
    assert "[initial_value] is an invalid option" in capsys.readouterr().out


def test_duplicate_numbers_are_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("duplicate_numbers.yaml")
    assert read_config({}) is None
    assert (
        "Only one TMP102 number may reference each temperature sensor"
        in capsys.readouterr().out
    )


@pytest.mark.parametrize(
    ("extended", "high", "low"),
    [
        (False, 127.9375, -55),
        (True, 150, -55),
        (False, 30, 30),
    ],
)
def test_valid_limits(extended: bool, high: float, low: float) -> None:
    validated = CONFIG_SCHEMA(
        {
            "name": "Temperature",
            "extended_mode": extended,
            "temperature_high": high,
            "temperature_low": low,
        }
    )
    assert validated["temperature_high"] == high
    assert validated["temperature_low"] == low


@pytest.mark.parametrize(
    "config",
    [
        {"temperature_high": float("nan")},
        {"temperature_low": float("inf")},
        {"temperature_low": -55.0625},
        {"temperature_high": 128},
        {"extended_mode": True, "temperature_high": 150.0625},
        {"temperature_high": 30, "temperature_low": 31},
        {"temperature_high": 74},
        {"temperature_low": 81},
        {"fault_queue": 3},
    ],
)
def test_invalid_limits(config: dict) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"name": "Temperature", **config})


def test_precomputed_register_values() -> None:
    config = CONFIG_SCHEMA(
        {
            "name": "Temperature",
            "extended_mode": True,
            "conversion_rate": "0.25Hz",
            "one_shot_mode": True,
            "alert_polarity": "active_high",
            "thermostat_mode": "interrupt",
            "fault_queue": 6,
            "temperature_high": 12.22,
            "temperature_low": -12.22,
        }
    )
    assert component.build_configuration(config) == 0x1F10
    assert component.encode_temperature(config["temperature_high"], True) == 0x0620
    assert component.encode_temperature(config["temperature_low"], True) == 0xF9E0


def test_threshold_encoding_rounds_half_steps_away_from_zero() -> None:
    assert component.encode_temperature(30.03125, False) == 481 << 4
    assert component.encode_temperature(-30.03125, False) == (-481 << 4) & 0xFFFF
    assert component.encode_temperature(30.03125, True) == 481 << 3
    assert component.encode_temperature(-30.03125, True) == (-481 << 3) & 0xFFFF


def test_defaults_match_chip_reset() -> None:
    assert component.build_configuration({}) == 0x0080
