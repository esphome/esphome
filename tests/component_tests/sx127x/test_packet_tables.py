"""Tests for SX127x constant packets in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.sx127x import CONFIG_SCHEMA, validate_packet_data
import esphome.config_validation as cv


def test_constant_packets_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal constant packets share one table; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("packet_tables.yaml"))

    tables = dict(
        re.findall(
            r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
        )
    )
    assert sorted(tables.values()) == sorted(
        ["{0xC5, 0x51, 0x78, 0x82}", "{0x68, 0x69}"]
    )
    shared = next(k for k, v in tables.items() if v == "{0xC5, 0x51, 0x78, 0x82}")
    assert main_cpp.count(f"set_data_static({shared}, 4);") == 2
    assert "set_data_template(" in main_cpp


def test_packet_data_length_limit() -> None:
    """Constant packets are limited to the 255 bytes the length register holds."""
    assert len(validate_packet_data([0x01] * 255)) == 255
    with pytest.raises(cv.Invalid):
        validate_packet_data([0x01] * 256)
    with pytest.raises(cv.Invalid):
        validate_packet_data("x" * 256)
    with pytest.raises(cv.Invalid):
        validate_packet_data([])


def test_payload_length_limit() -> None:
    """payload_length goes into an 8 bit register, so 256 is rejected."""
    validator = next(
        v for k, v in CONFIG_SCHEMA.schema.items() if str(k) == "payload_length"
    )
    assert validator(255) == 255
    with pytest.raises(cv.Invalid):
        validator(256)
