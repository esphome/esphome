"""Tests for the hoermann_hcp config schema (Modbus address validation)."""

import pytest

from esphome.components import hoermann_hcp
import esphome.config_validation as cv

RESERVED_ADDRESSES = [0x01, 0x0A, 0x0F]
OUT_OF_RANGE_ADDRESSES = [0x00, 0xF4, 0xF8, 0xFE, 0xFF]
# 0x03 is the first free slot, 0xF3 is the inclusive upper bound (HCP2_MAX_ADDRESS).
VALID_ADDRESSES = [0x1F, 0x2A, 0xF0, 0xF3]


def test_default_address_is_0x2a() -> None:
    config = hoermann_hcp.CONFIG_SCHEMA({})
    assert config["address"] == 0x2A


@pytest.mark.parametrize("address", RESERVED_ADDRESSES)
def test_rejects_reserved_addresses(address) -> None:
    with pytest.raises(cv.Invalid, match="fixed to an official"):
        hoermann_hcp.CONFIG_SCHEMA({"address": address})


@pytest.mark.parametrize("address", OUT_OF_RANGE_ADDRESSES)
def test_rejects_out_of_range_addresses(address) -> None:
    # Match the Range validator's wording so the test proves 0xF4 etc. are rejected
    # by the range check, not by something unrelated.
    with pytest.raises(cv.Invalid, match="value must be"):
        hoermann_hcp.CONFIG_SCHEMA({"address": address})


@pytest.mark.parametrize("address", VALID_ADDRESSES)
def test_accepts_valid_addresses(address) -> None:
    config = hoermann_hcp.CONFIG_SCHEMA({"address": address})
    assert config["address"] == address


def test_accepts_deprecated_0x02_with_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """0x02 is the old default; it still builds during the deprecation window but warns."""
    config = hoermann_hcp.CONFIG_SCHEMA({"address": 0x02})
    assert config["address"] == 0x02
    assert "address: 0x02' is deprecated" in caplog.text
