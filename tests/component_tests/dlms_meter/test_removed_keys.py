"""Tests for the dlms_meter predefined keys removed in 2026.11.0."""

from collections.abc import Callable

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.dlms_meter.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.components.dlms_meter.text_sensor import (
    CONFIG_SCHEMA as TEXT_SENSOR_SCHEMA,
)
from esphome.types import ConfigType


def _errors(schema: Callable[[ConfigType], ConfigType], config: ConfigType) -> str:
    with pytest.raises(Invalid) as exc_info:
        schema(config)
    errors = (
        exc_info.value.errors
        if isinstance(exc_info.value, MultipleInvalid)
        else [exc_info.value]
    )
    return "\n".join(str(error) for error in errors)


def test_removed_sensor_key_names_its_replacement() -> None:
    errors = _errors(SENSOR_SCHEMA, {"voltage_l1": {"name": "Voltage"}})
    assert 'obis_code: "1.0.32.7.0.255" and unit_of_measurement: V' in errors


def test_removed_text_sensor_key_names_its_replacement() -> None:
    errors = _errors(TEXT_SENSOR_SCHEMA, {"timestamp": {"name": "Timestamp"}})
    assert 'obis_code: "0.0.1.0.0.255"' in errors
