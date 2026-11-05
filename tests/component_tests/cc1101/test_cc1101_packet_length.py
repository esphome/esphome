"""Tests for cc1101 packet_length validation."""

from typing import Any

import pytest

from esphome.components.cc1101 import _validate_packet_mode
import esphome.config_validation as cv
from esphome.core import Lambda

LENGTH_LAMBDA = Lambda("return x.size() < 3 ? 0 : x[2] + 12;")


@pytest.mark.parametrize(
    "config",
    [
        {"packet_mode": True, "gdo0_pin": 2, "packet_length": LENGTH_LAMBDA},
        {"packet_mode": True, "gdo0_pin": 2, "packet_length": 1024},
        {"packet_mode": True, "gdo0_pin": 2, "packet_length": 64, "crc_enable": True},
        {"packet_mode": False, "packet_length": 200, "crc_enable": True},
    ],
)
def test_packet_length_valid(config: dict[str, Any]) -> None:
    """Fixed lengths, a lambda, and CRC on packets that fit the FIFO are accepted."""
    assert _validate_packet_mode(config) is config


@pytest.mark.parametrize(
    ("config", "message"),
    [
        (
            {"packet_mode": False, "packet_length": LENGTH_LAMBDA},
            "packet_mode is required when packet_length is a lambda",
        ),
        (
            {"packet_mode": True, "gdo0_pin": 2},
            "packet_length is required when packet_mode is enabled",
        ),
        (
            {
                "packet_mode": True,
                "gdo0_pin": 2,
                "packet_length": 65,
                "crc_enable": True,
            },
            "crc_enable is not available",
        ),
        (
            {
                "packet_mode": True,
                "gdo0_pin": 2,
                "packet_length": LENGTH_LAMBDA,
                "crc_enable": True,
            },
            "crc_enable is not available",
        ),
    ],
)
def test_packet_length_invalid(config: dict[str, Any], message: str) -> None:
    """Invalid packet_length combinations are rejected with a clear message."""
    with pytest.raises(cv.Invalid, match=message):
        _validate_packet_mode(config)
