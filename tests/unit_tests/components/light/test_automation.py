"""Tests for the light automation module's config validators."""

import pytest

from esphome.components.light.automation import validate_light_state
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("ON", True),
        ("on", True),
        ("OFF", False),
        ("off", False),
        (True, True),
        (False, False),
        ("true", True),
        ("false", False),
        ("yes", True),
        ("no", False),
        ("enable", True),
        ("disable", False),
    ],
)
def test_validate_light_state_accepts_documented_and_legacy_forms(
    value: bool | str, expected: bool
) -> None:
    assert validate_light_state(value) is expected


def test_validate_light_state_rejects_unknown_string() -> None:
    with pytest.raises(cv.Invalid):
        validate_light_state("bogus")
