"""Tests for cv.percentage_int."""

import pytest

from esphome import config_validation as cv


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        (85, 85),
        (0, 0),
        ("85", 85),
        ("85%", 85),
        ("85 %", 85),
        ("0%", 0),
    ],
)
def test_percentage_int_valid(value: int | str, expected: int) -> None:
    assert cv.percentage_int(value) == expected


@pytest.mark.parametrize("value", ["abc", "85.5", "85.5%", "%", ""])
def test_percentage_int_invalid(value: str) -> None:
    with pytest.raises(cv.Invalid):
        cv.percentage_int(value)
