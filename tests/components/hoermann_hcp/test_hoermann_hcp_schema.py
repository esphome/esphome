"""Tests for the hoermann_hcp config schema (Modbus address validation)."""

import pytest

from esphome.components import hoermann_hcp
import esphome.config_validation as cv

RESERVED_ADDRESSES = [0x01, 0x02, 0x0A, 0x0F]
OUT_OF_RANGE_ADDRESSES = [0x00, 0xF4, 0xF8, 0xFE, 0xFF]
VALID_ADDRESSES = [0x1F, 0x2A, 0xF0]


def test_default_address_is_0x2a() -> None:
    config = hoermann_hcp.CONFIG_SCHEMA({})
    assert config["address"] == 0x2A


@pytest.mark.parametrize("address", RESERVED_ADDRESSES)
def test_rejects_reserved_addresses(address) -> None:
    with pytest.raises(cv.Invalid, match="fixed to an official"):
        hoermann_hcp.CONFIG_SCHEMA({"address": address})


@pytest.mark.parametrize("address", OUT_OF_RANGE_ADDRESSES)
def test_rejects_out_of_range_addresses(address) -> None:
    with pytest.raises(cv.Invalid):
        hoermann_hcp.CONFIG_SCHEMA({"address": address})


@pytest.mark.parametrize("address", VALID_ADDRESSES)
def test_accepts_valid_addresses(address) -> None:
    config = hoermann_hcp.CONFIG_SCHEMA({"address": address})
    assert config["address"] == address
