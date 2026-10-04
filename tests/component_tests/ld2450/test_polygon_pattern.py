"""Tests for the polygon zone text pattern and maximum length.

Home Assistant validates polygon input with the pattern before it reaches the device, so the pattern must accept
what Polygon::parse() accepts. Coordinate ranges are the one exception: only the device checks those.
"""

from __future__ import annotations

import re

import pytest

from esphome.components.ld2450.text import (
    MAX_POLYGON_POINTS,
    POLYGON_MAX_LENGTH,
    POLYGON_PATTERN,
)

# The same inputs as the C++ parser tests (ld2450_polygon.cpp), without out-of-range values
ACCEPTED = [
    "",
    "   ",
    "-1000,500;1000,500;0,3000",
    " -1000 , 500 ; 1000,500;  0,3000; ",
    "-4860,0;4860,0;4860,7560",
]
REJECTED = [
    "0,0",
    "0,0;100,100",
    "junk",
    "1,2;3",
    "1,2;3,4;5,",
    "1;2;3",
    "1,2 3,4 5,6",
    "1,2;;3,4;5,6",
    "--1,2;3,4;5,6",
    "1,2;3,4;5,6x",
    "1,2;\t3,4;5,6",
]


@pytest.mark.parametrize("value", ACCEPTED)
def test_pattern_accepts_valid_polygons(value: str) -> None:
    assert re.fullmatch(POLYGON_PATTERN, value)


@pytest.mark.parametrize("value", REJECTED)
def test_pattern_rejects_invalid_polygons(value: str) -> None:
    assert not re.fullmatch(POLYGON_PATTERN, value)


def test_point_count_limits() -> None:
    assert re.fullmatch(POLYGON_PATTERN, ";".join(["0,0"] * MAX_POLYGON_POINTS))
    assert not re.fullmatch(
        POLYGON_PATTERN, ";".join(["0,0"] * (MAX_POLYGON_POINTS + 1))
    )


def test_longest_polygon_fits_max_length() -> None:
    longest = ";".join(["-4860,7560"] * MAX_POLYGON_POINTS)
    assert re.fullmatch(POLYGON_PATTERN, longest)
    assert len(longest) == POLYGON_MAX_LENGTH
