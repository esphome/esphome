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


def test_threshold_numbers_enable_configuration_once(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("numbers.yaml"))
    assert source.count("set_configuration(") == 1
    assert "set_configuration(128, 7680, 7168)" in source
    assert "set_high_limit_control" in source
    assert "set_low_limit_control" in source
    assert source.count("set_initial_value(") == 2
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_NUMBER" in defines
    assert "USE_TMP102_CONFIGURE" in defines


def test_threshold_numbers_inherit_parent_values(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("numbers_inherit.yaml"))
    assert source.count("set_configuration(") == 1
    assert "set_configuration(144, 17920, 15360)" in source
    assert "set_initial_value(140.0f)" in source
    assert "set_initial_value(120.0f)" in source


@pytest.mark.parametrize(
    ("filename", "message"),
    [
        (
            "numbers_conflict.yaml",
            "Set temperature_high on the sensor or initial_value on its number, not both",
        ),
        ("numbers_inverted.yaml", "low limit (25.0°C) must be <= high limit (20.0°C)"),
        (
            "duplicate_numbers.yaml",
            "Only one TMP102 number may reference each temperature sensor",
        ),
        (
            "numbers_high_below_default.yaml",
            (
                "high limit (50.0°C) is below the chip power-up TLOW default "
                "(75.0°C). Set temperature_low explicitly."
            ),
        ),
    ],
)
def test_invalid_number_configurations_are_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    filename: str,
    message: str,
) -> None:
    CORE.config_path = component_config_path(filename)
    assert read_config({}) is None
    assert message in capsys.readouterr().out


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


def test_defaults_match_chip_reset() -> None:
    assert component.build_configuration({}) == 0x0080
