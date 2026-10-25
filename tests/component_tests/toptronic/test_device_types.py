"""Tests for the toptronic device type table."""

import pytest

from esphome.components.toptronic import DEVICE_TYPES


@pytest.mark.parametrize("device_type", DEVICE_TYPES)
def test_device_type_has_no_address_bits(device_type: str) -> None:
    """The address is ORed into the low 6 bits, so a type base must leave them clear."""
    assert DEVICE_TYPES[device_type] & 0x3F == 0


def test_every_gateway_address_is_distinct() -> None:
    ids = {DEVICE_TYPES["GW"] | addr for addr in range(64)}
    assert len(ids) == 64
