"""Tests for the misspelled combination key removed in 2026.12.0."""

import pytest
from voluptuous import Invalid

from esphome.components.combination.sensor import LINEAR_SOURCE_SCHEMA


def test_coeffecient_names_coefficient() -> None:
    with pytest.raises(Invalid, match="use 'coefficient' instead"):
        LINEAR_SOURCE_SCHEMA({"source": "temp", "coefficient": 1, "coeffecient": 0.5})
