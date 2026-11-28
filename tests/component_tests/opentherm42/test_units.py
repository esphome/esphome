"""Tests that opentherm42 entity units are ones Home Assistant accepts for their device class."""

import importlib

import pytest
import voluptuous as vol

from esphome import config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_UNIT_OF_MEASUREMENT

# Units Home Assistant accepts per device class, for the device classes this component uses.
# A micro unit must use the Greek mu (U+03BC): ESPHome's own UNIT_MICROAMP uses the micro sign
# (U+00B5), which Home Assistant rejects for the "current" device class.
HA_DEVICE_CLASS_UNITS: dict[str, set[str]] = {
    "carbon_dioxide": {"ppm"},
    "current": {"\u03bcA", "mA", "A"},
    "duration": {"h", "ms"},
    "energy": {"kWh"},
    "humidity": {"%"},
    "power": {"W", "kW"},
    "pressure": {"bar"},
    "temperature": {"°C"},
    "volume_flow_rate": {"L/min"},
}

MICRO_SIGN = "\u00b5"


def _default(schema: cv.Schema, key: str) -> str | None:
    """Return the default a schema declares for key, or None when it has none."""
    for marker in schema.schema:
        if (
            getattr(marker, "schema", None) == key
            and marker.default is not vol.UNDEFINED
        ):
            return marker.default()
    return None


def _entity_schemas() -> list[tuple[str, cv.Schema]]:
    """Every sensor and number schema declared by the opentherm42 platforms."""
    schemas: list[tuple[str, cv.Schema]] = []
    sensor_module = importlib.import_module("esphome.components.opentherm42.sensor")
    for marker, schema in sensor_module.TYPES.items():
        schemas.append((f"sensor.{marker}", schema))
    number_module = importlib.import_module("esphome.components.opentherm42.number")
    for table_name in ("TYPES", "SENSOR_FEED_TYPES"):
        for marker, entry in getattr(number_module, table_name, {}).items():
            schemas.append((f"number.{marker}", entry[0]))
    return schemas


@pytest.mark.parametrize(("name", "schema"), _entity_schemas())
def test_unit_is_accepted_by_home_assistant(name: str, schema: cv.Schema) -> None:
    unit = _default(schema, CONF_UNIT_OF_MEASUREMENT)
    device_class = _default(schema, CONF_DEVICE_CLASS)

    if unit is not None:
        assert MICRO_SIGN not in unit, (
            f"{name}: use Greek mu (U+03BC), not the micro sign"
        )
    if device_class is None:
        return
    assert device_class in HA_DEVICE_CLASS_UNITS, (
        f"{name}: add device class {device_class!r} to HA_DEVICE_CLASS_UNITS"
    )
    assert unit in HA_DEVICE_CLASS_UNITS[device_class], (
        f"{name}: unit {unit!r} is not valid for device class {device_class!r}"
    )
