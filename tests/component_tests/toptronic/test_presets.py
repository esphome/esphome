"""Tests for finding a hub's preset directory."""

import pytest

from esphome.components.toptronic import _preset_device_type


@pytest.mark.parametrize(
    ("value", "expected"), [("BD", "BM"), ("bd", "BM"), ("hv", "HV"), ("HV", "HV")]
)
def test_preset_device_type_before_validation(value: str, expected: str) -> None:
    assert _preset_device_type({"device_type": value}) == expected
