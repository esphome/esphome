"""Tests for SX126x constant packets in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.sx126x import validate_packet_data
import esphome.config_validation as cv


def test_constant_packets_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal constant packets share one table; a lambda is sent from its vector."""
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
    assert main_cpp.count(f"->transmit_packet({shared}, 4);") == 2
    assert (
        "->transmit_packet(esphome__bytes.data(), esphome__bytes.size());" in main_cpp
    )
    assert "set_sx1261" not in main_cpp


def test_packet_data_length_limit() -> None:
    """Constant packets must be 1 to 255 bytes long."""
    assert len(validate_packet_data([0x01] * 255)) == 255
    with pytest.raises(cv.Invalid):
        validate_packet_data([0x01] * 256)
    with pytest.raises(cv.Invalid):
        validate_packet_data([])


def test_hw_version_sx1261_sets_flag(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An sx1261 sets the flag."""
    main_cpp = generate_main(component_config_path("hw_version_sx1261.yaml"))
    assert "set_sx1261(true);" in main_cpp
