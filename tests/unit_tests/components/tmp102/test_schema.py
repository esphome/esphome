"""Validate TMP102 limits and upstream-compatible sensor defaults."""

import pytest

from esphome.components import tmp102 as component
from esphome.components.tmp102.sensor import CONFIG_SCHEMA
import esphome.config_validation as cv


@pytest.mark.parametrize(
    "extended,high,low",
    [
        (False, 127.9375, -55),
        (True, 150, -55),
        (False, 30, 30),
    ],
)
def test_valid_limits(extended: bool, high: float, low: float) -> None:
    config = {
        "extended_mode": extended,
        "temperature_high": high,
        "temperature_low": low,
    }
    assert component.validate_tmp102_thresholds(config) == config


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
    ],
)
def test_invalid_limits(config: dict) -> None:
    with pytest.raises(cv.Invalid):
        component.validate_tmp102_thresholds(config)


def test_upstream_defaults() -> None:
    config = CONFIG_SCHEMA({"name": "Temperature"})
    assert config["address"] == 0x48
    assert config["update_interval"].total_milliseconds == 60000
    assert config["accuracy_decimals"] == 1
    assert config["unit_of_measurement"] == "°C"
    assert config["device_class"] == "temperature"
    assert config["state_class"] == "measurement"
    assert "extended_mode" not in config
    assert "temperature_high" not in config


@pytest.mark.parametrize("address", [0x48, 0x49, 0x4A, 0x4B, 0x4C])
def test_upstream_address_validation_unchanged(address: int) -> None:
    # Do not narrow the I2C address validator accepted by upstream tmp102.
    assert (
        CONFIG_SCHEMA({"name": "Temperature", "address": address})["address"] == address
    )


@pytest.mark.parametrize(
    "key,value",
    [
        ("alert", {"name": "Alert"}),
        ("threshold_status", {"name": "Status"}),
        ("temperature_high", {"name": "High"}),
    ],
)
def test_old_nested_entities_require_migration(key: str, value: dict) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"name": "Temperature", key: value})
