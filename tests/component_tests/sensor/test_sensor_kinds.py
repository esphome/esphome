"""Tests for the sensor_schema kind presets."""

from typing import Any

import pytest

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILTERS,
    CONF_ID,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_HECTOPASCAL,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

_M = STATE_CLASS_MEASUREMENT
_T = STATE_CLASS_TOTAL_INCREASING
_PM = UNIT_MICROGRAMS_PER_CUBIC_METER
_NO_ICON = cv.UNDEFINED

# (unit, device class, state class, icon) each preset must expand to
EXPECTED = {
    "TEMPERATURE": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, _M, _NO_ICON),
    "HUMIDITY": (UNIT_PERCENT, DEVICE_CLASS_HUMIDITY, _M, _NO_ICON),
    "BATTERY": (UNIT_PERCENT, DEVICE_CLASS_BATTERY, _M, _NO_ICON),
    "PRESSURE": (UNIT_HECTOPASCAL, DEVICE_CLASS_PRESSURE, _M, _NO_ICON),
    "ILLUMINANCE": (UNIT_LUX, DEVICE_CLASS_ILLUMINANCE, _M, _NO_ICON),
    "CARBON_DIOXIDE": (
        UNIT_PARTS_PER_MILLION,
        DEVICE_CLASS_CARBON_DIOXIDE,
        _M,
        _NO_ICON,
    ),
    "PM1": (_PM, DEVICE_CLASS_PM1, _M, ICON_CHEMICAL_WEAPON),
    "PM25": (_PM, DEVICE_CLASS_PM25, _M, ICON_CHEMICAL_WEAPON),
    "PM10": (_PM, DEVICE_CLASS_PM10, _M, ICON_CHEMICAL_WEAPON),
    "SIGNAL_STRENGTH": (
        UNIT_DECIBEL_MILLIWATT,
        DEVICE_CLASS_SIGNAL_STRENGTH,
        _M,
        _NO_ICON,
    ),
    "VOLTAGE": (UNIT_VOLT, DEVICE_CLASS_VOLTAGE, _M, _NO_ICON),
    "CURRENT": (UNIT_AMPERE, DEVICE_CLASS_CURRENT, _M, _NO_ICON),
    "POWER": (UNIT_WATT, DEVICE_CLASS_POWER, _M, _NO_ICON),
    "APPARENT_POWER": (UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER, _M, _NO_ICON),
    "REACTIVE_POWER": (
        UNIT_VOLT_AMPS_REACTIVE,
        DEVICE_CLASS_REACTIVE_POWER,
        _M,
        _NO_ICON,
    ),
    "FREQUENCY": (UNIT_HERTZ, DEVICE_CLASS_FREQUENCY, _M, _NO_ICON),
    "ENERGY_KWH": (UNIT_KILOWATT_HOURS, DEVICE_CLASS_ENERGY, _T, _NO_ICON),
    "ENERGY_WH": (UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, _T, _NO_ICON),
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


def test_every_preset_is_pinned() -> None:
    kinds = {
        name
        for name, value in vars(sensor).items()
        if isinstance(value, sensor.SensorKind)
    }
    assert kinds == EXPECTED.keys()


@pytest.mark.parametrize("name", sorted(EXPECTED))
def test_kind_matches_hand_written(name: str) -> None:
    unit, device_class, state_class, icon = EXPECTED[name]
    by_kind = sensor.sensor_schema(kind=getattr(sensor, name), accuracy_decimals=1)
    by_hand = sensor.sensor_schema(
        unit_of_measurement=unit,
        icon=icon,
        accuracy_decimals=1,
        device_class=device_class,
        state_class=state_class,
    )
    assert _keys(by_kind) == _keys(by_hand)
    assert _defaults(by_kind) == _defaults(by_hand)


def test_extra_arguments_pass_through() -> None:
    schema = sensor.sensor_schema(
        sensor.Sensor,
        kind=sensor.TEMPERATURE,
        accuracy_decimals=2,
        icon="mdi:thermometer",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        filters=[{"offset": 1.0}],
    )
    defaults = _defaults(schema)
    assert (
        defaults.items()
        >= {
            "unit_of_measurement": UNIT_CELSIUS,
            "icon": "mdi:thermometer",
            "accuracy_decimals": 2,
            "device_class": DEVICE_CLASS_TEMPERATURE,
            "state_class": STATE_CLASS_MEASUREMENT,
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
            CONF_FILTERS: [{"offset": 1.0}],
        }.items()
    )
    # class_ replaces the default sensor ID declaration
    ids = [v for k, v in schema.schema.items() if str(k) == CONF_ID]
    default_ids = [
        v for k, v in sensor.sensor_schema().schema.items() if str(k) == CONF_ID
    ]
    assert len(ids) == 1
    assert ids != default_ids


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
