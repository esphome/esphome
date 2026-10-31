"""Tests for the sensor_schema kind presets."""

from typing import Any

import pytest

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHEMICAL_WEAPON

KINDS = {
    name: value
    for name, value in vars(sensor).items()
    if isinstance(value, sensor.SensorKind)
}


def _keys(schema: cv.Schema) -> list[tuple[Any, ...]]:
    """Key type, name and validator of every schema entry, in order."""
    return [
        (type(key), str(key), validator) for key, validator in schema.schema.items()
    ]


def _defaults(schema: cv.Schema) -> dict[str, Any]:
    return {
        str(key): key.default()
        for key in schema.schema
        if isinstance(key, cv.Optional) and key.default is not cv.UNDEFINED
    }


def test_presets_found() -> None:
    assert len(KINDS) == 18


@pytest.mark.parametrize("name", sorted(KINDS))
def test_kind_matches_hand_written(name: str) -> None:
    kind = KINDS[name]
    by_kind = sensor.sensor_schema(kind=kind, accuracy_decimals=1)
    by_hand = sensor.sensor_schema(
        unit_of_measurement=kind.unit_of_measurement,
        icon=kind.icon,
        accuracy_decimals=1,
        device_class=kind.device_class,
        state_class=kind.state_class,
    )
    assert _keys(by_kind) == _keys(by_hand)
    assert _defaults(by_kind) == _defaults(by_hand)


def test_preset_icon() -> None:
    assert _defaults(sensor.sensor_schema(kind=sensor.PM10))["icon"] == (
        ICON_CHEMICAL_WEAPON
    )
    assert "icon" not in _defaults(sensor.sensor_schema(kind=sensor.TEMPERATURE))


def test_extra_arguments_pass_through() -> None:
    schema = sensor.sensor_schema(
        kind=sensor.TEMPERATURE,
        accuracy_decimals=2,
        icon="mdi:thermometer",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    assert (
        _defaults(schema).items()
        >= {
            "unit_of_measurement": "°C",
            "icon": "mdi:thermometer",
            "accuracy_decimals": 2,
            "device_class": "temperature",
            "state_class": "measurement",
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        }.items()
    )


@pytest.mark.parametrize(
    ("kind", "key"),
    [
        (sensor.TEMPERATURE, "unit_of_measurement"),
        (sensor.TEMPERATURE, "device_class"),
        (sensor.TEMPERATURE, "state_class"),
        (sensor.PM10, "icon"),
    ],
)
def test_kind_with_explicit_value_is_rejected(
    kind: sensor.SensorKind, key: str
) -> None:
    with pytest.raises(ValueError, match=f"kind already sets {key}"):
        sensor.sensor_schema(kind=kind, **{key: "x"})
