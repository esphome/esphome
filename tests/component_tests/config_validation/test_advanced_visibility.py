"""Power-user fields are marked as advanced on the shared schemas.

``filters``, ``manual_ip`` and the GPIO switch interlock options are knobs
whose defaults suit nearly every user, so a schema-aware editor should keep
them behind its "advanced settings" disclosure rather than on the main form.
"""

from __future__ import annotations

import importlib

import pytest

from esphome.components import binary_sensor, ethernet, sensor, text_sensor, wifi
import esphome.config_validation as cv


def _markers(schema: cv.Schema) -> dict[str, object]:
    s = schema
    if hasattr(s, "validators"):
        # cv.All -> the schema is the first validator.
        s = s.validators[0]
    return {str(k): k for k in s.schema}


def _gpio_switch_schema() -> cv.Schema:
    return importlib.import_module("esphome.components.gpio.switch").CONFIG_SCHEMA


@pytest.mark.parametrize(
    ("label", "schema_factory", "fields"),
    [
        ("sensor", sensor.sensor_schema, ["filters"]),
        ("binary_sensor", binary_sensor.binary_sensor_schema, ["filters"]),
        ("text_sensor", text_sensor.text_sensor_schema, ["filters"]),
        ("wifi_network", lambda: wifi.WIFI_NETWORK_BASE, ["manual_ip"]),
        ("wifi", lambda: wifi.CONFIG_SCHEMA, ["manual_ip"]),
        ("ethernet", lambda: ethernet.BASE_SCHEMA, ["manual_ip"]),
        ("gpio_switch", _gpio_switch_schema, ["interlock", "interlock_wait_time"]),
    ],
)
def test_power_user_fields_are_advanced(
    label: str, schema_factory, fields: list[str]
) -> None:
    markers = _markers(schema_factory())
    for field in fields:
        assert markers[field].visibility is cv.Visibility.ADVANCED, f"{label}.{field}"


def test_interlock_wait_time_keeps_its_default() -> None:
    """Marking the field advanced must not drop its default."""
    markers = _markers(_gpio_switch_schema())
    assert markers["interlock_wait_time"].default() == "0ms"
