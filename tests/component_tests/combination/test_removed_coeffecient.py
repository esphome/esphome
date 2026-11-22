"""Tests for the misspelled combination key removed in 2026.12.0."""

import pytest
from voluptuous import MultipleInvalid

from esphome.components.combination.sensor import LINEAR_SOURCE_SCHEMA


def test_coeffecient_names_coefficient() -> None:
    with pytest.raises(MultipleInvalid) as exc_info:
        LINEAR_SOURCE_SCHEMA({"source": "temp", "coeffecient": 0.5})
    errors = "\n".join(str(error) for error in exc_info.value.errors)
    assert "use 'coefficient' instead" in errors
