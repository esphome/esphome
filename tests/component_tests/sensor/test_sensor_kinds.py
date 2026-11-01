"""Tests for the sensor_schema kind presets."""

from typing import Any

import pytest

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHEMICAL_WEAPON,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_DEGREES,
    UNIT_HECTOPASCAL,
    UNIT_HERTZ,
    UNIT_HOUR,
    UNIT_KILOVOLT_AMPS,
    UNIT_KILOVOLT_AMPS_REACTIVE,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_MILLIMETER,
    UNIT_MINUTE,
    UNIT_OHM,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

_M = STATE_CLASS_MEASUREMENT
_T = STATE_CLASS_TOTAL_INCREASING
_PM = UNIT_MICROGRAMS_PER_CUBIC_METER
_UNSET = cv.UNDEFINED

# (unit, device class, state class, icon, accuracy decimals) each preset must expand to
EXPECTED = {
    "TEMPERATURE": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, _M, _UNSET, 1),
    "HUMIDITY": (UNIT_PERCENT, DEVICE_CLASS_HUMIDITY, _M, _UNSET, 1),
    "BATTERY": (UNIT_PERCENT, DEVICE_CLASS_BATTERY, _M, _UNSET, 0),
    "PRESSURE": (UNIT_HECTOPASCAL, DEVICE_CLASS_PRESSURE, _M, _UNSET, 1),
    "ILLUMINANCE": (UNIT_LUX, DEVICE_CLASS_ILLUMINANCE, _M, _UNSET, 0),
    "CARBON_DIOXIDE": (
        UNIT_PARTS_PER_MILLION,
        DEVICE_CLASS_CARBON_DIOXIDE,
        _M,
        _UNSET,
        0,
    ),
    "PM1": (_PM, DEVICE_CLASS_PM1, _M, ICON_CHEMICAL_WEAPON, 0),
    "PM25": (_PM, DEVICE_CLASS_PM25, _M, ICON_CHEMICAL_WEAPON, 0),
    "PM10": (_PM, DEVICE_CLASS_PM10, _M, ICON_CHEMICAL_WEAPON, 0),
    "SIGNAL_STRENGTH": (
        UNIT_DECIBEL_MILLIWATT,
        DEVICE_CLASS_SIGNAL_STRENGTH,
        _M,
        _UNSET,
        0,
    ),
    "VOLTAGE": (UNIT_VOLT, DEVICE_CLASS_VOLTAGE, _M, _UNSET, _UNSET),
    "CURRENT": (UNIT_AMPERE, DEVICE_CLASS_CURRENT, _M, _UNSET, _UNSET),
    "POWER": (UNIT_WATT, DEVICE_CLASS_POWER, _M, _UNSET, _UNSET),
    "APPARENT_POWER": (
        UNIT_VOLT_AMPS,
        DEVICE_CLASS_APPARENT_POWER,
        _M,
        _UNSET,
        _UNSET,
    ),
    "REACTIVE_POWER": (
        UNIT_VOLT_AMPS_REACTIVE,
        DEVICE_CLASS_REACTIVE_POWER,
        _M,
        _UNSET,
        _UNSET,
    ),
    "FREQUENCY": (UNIT_HERTZ, DEVICE_CLASS_FREQUENCY, _M, _UNSET, _UNSET),
    "ENERGY_KWH": (UNIT_KILOWATT_HOURS, DEVICE_CLASS_ENERGY, _T, _UNSET, 3),
    "ENERGY_WH": (UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, _T, _UNSET, _UNSET),
    "POWER_KW": (UNIT_KILOWATT, DEVICE_CLASS_POWER, _M, _UNSET, 3),
    "APPARENT_POWER_KVA": (
        UNIT_KILOVOLT_AMPS,
        DEVICE_CLASS_APPARENT_POWER,
        _M,
        _UNSET,
        3,
    ),
    "REACTIVE_POWER_KVAR": (
        UNIT_KILOVOLT_AMPS_REACTIVE,
        DEVICE_CLASS_REACTIVE_POWER,
        _M,
        _UNSET,
        3,
    ),
    "REACTIVE_ENERGY_KVARH": (UNIT_KILOVOLT_AMPS_REACTIVE_HOURS, _UNSET, _T, _UNSET, 3),
    "POWER_FACTOR": (_UNSET, DEVICE_CLASS_POWER_FACTOR, _M, _UNSET, _UNSET),
    "PERCENT": (UNIT_PERCENT, _UNSET, _M, _UNSET, 0),
    "ANGLE": (UNIT_DEGREES, _UNSET, _M, _UNSET, _UNSET),
    "MASS_CONCENTRATION": (_PM, _UNSET, _M, _UNSET, 0),
    "LENGTH_M": (UNIT_METER, _UNSET, _M, _UNSET, 2),
    "DISTANCE_MM": (UNIT_MILLIMETER, DEVICE_CLASS_DISTANCE, _M, _UNSET, 0),
    "RESISTANCE": (UNIT_OHM, _UNSET, _M, _UNSET, 0),
    "DURATION_H": (UNIT_HOUR, DEVICE_CLASS_DURATION, _M, ICON_TIMER, 0),
    "DURATION_MIN": (UNIT_MINUTE, DEVICE_CLASS_DURATION, _M, _UNSET, 0),
    "DURATION_S": (UNIT_SECOND, DEVICE_CLASS_DURATION, _M, _UNSET, 0),
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
    unit, device_class, state_class, icon, decimals = EXPECTED[name]
    by_kind = sensor.sensor_schema(kind=getattr(sensor, name))
    by_hand = sensor.sensor_schema(
        unit_of_measurement=unit,
        icon=icon,
        accuracy_decimals=decimals,
        device_class=device_class,
        state_class=state_class,
    )
    assert _keys(by_kind) == _keys(by_hand)
    assert _defaults(by_kind) == _defaults(by_hand)


@pytest.mark.parametrize("kind", [sensor.TEMPERATURE, sensor.VOLTAGE])
def test_accuracy_decimals_can_be_overridden(kind: sensor.SensorKind) -> None:
    schema = sensor.sensor_schema(kind=kind, accuracy_decimals=3)
    assert _defaults(schema)["accuracy_decimals"] == 3


def test_extra_arguments_pass_through() -> None:
    extra = {
        "accuracy_decimals": 2,
        "icon": "mdi:thermometer",
        "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        "filters": [{"offset": 1.0}],
    }
    by_kind = sensor.sensor_schema(kind=sensor.TEMPERATURE, **extra)
    by_hand = sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        **extra,
    )
    assert _keys(by_kind) == _keys(by_hand)
    assert _defaults(by_kind) == _defaults(by_hand)


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
