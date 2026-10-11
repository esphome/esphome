"""The template platforms surface value-describing metadata on the main form.

Hardware platforms get sensible defaults for unit/device_class/etc., so those
fields fall through to the editor's advanced disclosure. A ``template`` entity
has no such defaults -- the user is expected to define them -- so the template
platforms pass ``visibility=cv.Visibility.UI`` to promote them onto the form.
"""

from __future__ import annotations

import importlib

import pytest

import esphome.config_validation as cv


def _markers(schema: cv.Schema) -> dict[str, object]:
    s = schema
    if hasattr(s, "validators"):
        # cv.All -> the schema is the first validator.
        s = s.validators[0]
    return {str(k): k for k in s.schema}


@pytest.mark.parametrize(
    ("platform", "fields"),
    [
        (
            "sensor",
            [
                "unit_of_measurement",
                "accuracy_decimals",
                "device_class",
                "state_class",
                "force_update",
            ],
        ),
        ("binary_sensor", ["device_class"]),
        ("switch", ["device_class"]),
        ("cover", ["device_class"]),
        ("button", ["device_class"]),
        ("valve", ["device_class"]),
        ("event", ["device_class"]),
        ("text_sensor", ["device_class"]),
        ("number", ["device_class", "unit_of_measurement"]),
    ],
)
def test_template_metadata_is_ui(platform: str, fields: list[str]) -> None:
    mod = importlib.import_module(f"esphome.components.template.{platform}")
    markers = _markers(mod.CONFIG_SCHEMA)
    for field in fields:
        assert markers[field].visibility is cv.Visibility.UI, f"{platform}.{field}"


def test_template_sensor_promotion_preserves_defaults() -> None:
    """Promoting to UI must not drop the fields' defaults."""
    from esphome.components.template.sensor import CONFIG_SCHEMA

    markers = _markers(CONFIG_SCHEMA)
    assert markers["accuracy_decimals"].default() == 1
    assert markers["force_update"].default() is False


def test_hardware_platform_metadata_not_promoted() -> None:
    """Without ``visibility=`` the builders leave metadata unset.

    Unset markers fall through to the consumer's ``Optional`` default of
    advanced, so hardware platforms are unaffected by the template promotion.
    """
    from esphome.components import binary_sensor, sensor

    hw_sensor = _markers(sensor.sensor_schema(device_class="temperature"))
    assert hw_sensor["device_class"].visibility is None
    hw_bs = _markers(binary_sensor.binary_sensor_schema(device_class="motion"))
    assert hw_bs["device_class"].visibility is None
