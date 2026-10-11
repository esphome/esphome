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
    validated = CONFIG_SCHEMA({"name": "Temperature", **config})
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
