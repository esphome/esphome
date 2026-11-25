"""Tests for the hm3301 aqi option removed in 2026.12.0."""

import pytest
from voluptuous import Invalid

from esphome.components.hm3301.sensor import CONFIG_SCHEMA


def test_aqi_names_the_aqi_platform() -> None:
    with pytest.raises(Invalid, match="'platform: aqi'"):
        CONFIG_SCHEMA({"aqi": {"name": "AQI", "calculation_type": "CAQI"}})
