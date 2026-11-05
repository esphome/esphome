"""Tests for sx127x payload_length validation."""

from typing import Any

import pytest

from esphome.components.sx127x import is_long_packet, validate_config
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


def test_dio0_pin_optional_for_long_packets() -> None:
    """Long packets do not use DIO0, so dio0_pin may be left out."""
    config = _config(payload_length=LENGTH_LAMBDA, dio1_pin=27)
    del config["dio0_pin"]
    assert validate_config(config) is config


def test_dio0_pin_required_for_short_packets() -> None:
    """Short packets in packet mode still need dio0_pin for PayloadReady."""
    config = _config(payload_length=8)
    del config["dio0_pin"]
    with pytest.raises(cv.Invalid, match="dio0_pin"):
        validate_config(config)


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
        (
            {"payload_length": 64, "dio1_pin": 27},
            "dio1_pin is only used with",
        ),
        (
            {"packet_mode": False, "dio1_pin": 27},
            "dio1_pin is only used with",
        ),
    ],
)
def test_payload_length_invalid(overrides: dict[str, Any], message: str) -> None:
    """Invalid payload_length combinations are rejected with a clear message."""
    with pytest.raises(cv.Invalid, match=message):
        validate_config(_config(**overrides))


@pytest.mark.parametrize(
    ("overrides", "expected"),
    [
        ({"payload_length": 64}, False),
        ({"payload_length": 65}, True),
        ({"payload_length": LENGTH_LAMBDA}, True),
        ({"packet_mode": False, "payload_length": 65}, False),
        ({"modulation": "LORA", "payload_length": 200}, False),
    ],
)
def test_is_long_packet(overrides: dict[str, Any], expected: bool) -> None:
    """Long packet RX applies to FSK/OOK packet mode above 64 bytes or with a lambda."""
    assert is_long_packet(_config(**overrides)) is expected
