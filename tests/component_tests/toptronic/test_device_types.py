"""Tests for the toptronic device type table."""

import pytest

from esphome.components.toptronic import DEVICE_TYPES, validate_device_type
import esphome.config_validation as cv


@pytest.mark.parametrize("device_type", DEVICE_TYPES)
def test_device_type_has_no_address_bits(device_type: str) -> None:
    """The address is ORed into the low 6 bits, so a type base must leave them clear."""
    assert DEVICE_TYPES[device_type] & 0x3F == 0


def test_every_gateway_address_is_distinct() -> None:
    ids = {DEVICE_TYPES["GW"] | addr for addr in range(64)}
    assert len(ids) == 64


@pytest.mark.parametrize("value", ["BD", "bd", "BM", "bm"])
def test_bd_is_an_alias_of_bm(value: str) -> None:
    assert validate_device_type(value) == "BM"


def test_unknown_device_type_is_rejected() -> None:
    with pytest.raises(cv.Invalid):
        validate_device_type("XYZ")
