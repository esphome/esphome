"""Exercise TMP102 sensor validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import tmp102 as component
from esphome.components.tmp102.sensor import CONFIG_SCHEMA
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
