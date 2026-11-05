"""Tests for sx127x payload_length validation."""

from typing import Any

import pytest

from esphome.components.sx127x import validate_config
import esphome.config_validation as cv
from esphome.core import Lambda

LENGTH_LAMBDA = Lambda("return x.size() < 3 ? 0 : x[2] + 12;")


def _config(**overrides: Any) -> dict[str, Any]:
    config = {
        "modulation": "FSK",
        "bandwidth": "125_0kHz",
        "bitsync": True,
        "packet_mode": True,
        "dio0_pin": 26,
        "payload_length": 0,
        "crc_enable": False,
        "pa_pin": "BOOST",
        "pa_power": 17,
        "preamble_size": 8,
        "spreading_factor": 7,
    }
    config.update(overrides)
    return config


@pytest.mark.parametrize(
    "overrides",
    [
        {"payload_length": 64, "crc_enable": True},
        {"payload_length": 1024, "dio1_pin": 27},
        {"payload_length": LENGTH_LAMBDA, "dio1_pin": 27},
        {"modulation": "LORA", "payload_length": 255},
    ],
)
def test_payload_length_valid(overrides: dict[str, Any]) -> None:
    """Short, long and lambda lengths are accepted where supported."""
    config = _config(**overrides)
    assert validate_config(config) is config


@pytest.mark.parametrize(
    ("overrides", "message"),
    [
        (
            {"packet_mode": False, "payload_length": 65},
            "payload_length must be <= 64 when packet_mode is disabled",
        ),
        (
            {"packet_mode": False, "payload_length": LENGTH_LAMBDA, "dio1_pin": 27},
            "packet_mode is required when payload_length is a lambda",
        ),
        (
            {"payload_length": 65},
            "dio1_pin is required",
        ),
        (
            {"payload_length": LENGTH_LAMBDA},
            "dio1_pin is required",
        ),
        (
            {"payload_length": 65, "dio1_pin": 27, "crc_enable": True},
            "crc_enable is not available",
        ),
        (
            {"modulation": "LORA", "payload_length": LENGTH_LAMBDA},
            "payload_length as a lambda is not available with LORA",
        ),
        (
            {"modulation": "LORA", "payload_length": 256},
            "Payload length must be <= 255 with LORA",
        ),
        (
            {"modulation": "LORA", "dio1_pin": 27},
            "dio1_pin is not available with LORA",
        ),
    ],
)
def test_payload_length_invalid(overrides: dict[str, Any], message: str) -> None:
    """Invalid payload_length combinations are rejected with a clear message."""
    with pytest.raises(cv.Invalid, match=message):
        validate_config(_config(**overrides))
