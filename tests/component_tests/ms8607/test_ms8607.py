"""Tests for the MS8607 sensor configuration."""

import pytest

from esphome import config_validation as cv
from esphome.components.ms8607.sensor import CONFIG_SCHEMA


def test_at_least_one_sensor_is_required() -> None:
    """A component without a sensor has nothing to publish."""
    with pytest.raises(cv.Invalid, match="Must contain at least one of"):
        CONFIG_SCHEMA({"id": "ms8607_sensor", "i2c_id": "i2c_bus"})


def test_single_sensor_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "ms8607_sensor",
            "i2c_id": "i2c_bus",
            "pressure": {"name": "MS8607 Pressure"},
        }
    )

    assert config["pressure"]["name"] == "MS8607 Pressure"
