"""Tests for the dlms_meter provider option removed in 2026.11.0."""

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.dlms_meter import CONFIG_SCHEMA


def test_removed_provider_shows_the_netznoe_patterns() -> None:
    with pytest.raises(Invalid) as exc_info:
        CONFIG_SCHEMA({"provider": "netznoe"})
    errors = (
        exc_info.value.errors
        if isinstance(exc_info.value, MultipleInvalid)
        else [exc_info.value]
    )
    assert any('pattern: "L, TSTR"' in str(error) for error in errors)
