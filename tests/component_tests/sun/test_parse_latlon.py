"""Tests for the sun component's latitude/longitude parsing."""

import pytest

from esphome import config_validation as cv
from esphome.components.sun import parse_latlon


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("51.5", 51.5),
        ("-74.0059", -74.0059),
        ("40°26'46\"N", 40.44611111111111),
        ("40°26'46\"S", -40.44611111111111),
        ("22°57′7″S", -22.951944444444443),
        ("22°57′7″s", -22.951944444444443),
        ("30°", 30.0),
        ("+40°N", 40.0),
        # Surrounding whitespace, e.g. from a YAML block scalar
        (" 40°26'46\"N\n", 40.44611111111111),
    ],
)
def test_parse_latlon_valid(value: str, expected: float) -> None:
    assert parse_latlon(value) == pytest.approx(expected)


@pytest.mark.parametrize(
    "value",
    [
        "abc",  # unexpanded secret/garbage used to silently parse as 0.0
        "S30",  # hemisphere letter before the number
        "30°30.5'S",  # decimal minutes are not supported: must error, not truncate
        "",
        "30 degrees",
        "40°26'46\"N and some trailing text",
        "-30°S",  # a sign and a hemisphere letter used to cancel out
    ],
)
def test_parse_latlon_invalid(value: str) -> None:
    with pytest.raises(cv.Invalid):
        parse_latlon(value)
