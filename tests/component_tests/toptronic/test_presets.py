"""Tests for finding a hub's preset directory."""

import pytest

from esphome.components.toptronic import AUTO_LOAD, _preset_device_type


@pytest.mark.parametrize(
    ("value", "expected"), [("BD", "BM"), ("bd", "BM"), ("hv", "HV"), ("HV", "HV")]
)
def test_preset_device_type_before_validation(value: str, expected: str) -> None:
    assert _preset_device_type({"device_type": value}) == expected


def test_lowercase_device_type_loads_preset_buttons() -> None:
    assert "button" in AUTO_LOAD([{"device_type": "hv"}])
