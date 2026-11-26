"""Tests for the misspelled combination key removed in 2026.12.0."""

import pytest
from voluptuous import MultipleInvalid

from esphome.components.combination.sensor import LINEAR_SOURCE_SCHEMA


def test_coeffecient_names_coefficient() -> None:
    """An old config with only the misspelled key is rejected with the new name."""
    with pytest.raises(MultipleInvalid) as exc_info:
        LINEAR_SOURCE_SCHEMA({"source": "temp", "coeffecient": 0.5})
    assert any("use 'coefficient' instead" in str(e) for e in exc_info.value.errors)
