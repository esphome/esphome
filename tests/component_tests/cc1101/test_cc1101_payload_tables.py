"""Tests for cc1101 constant payloads in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.cc1101 import SEND_PACKET_ACTION_SCHEMA
import esphome.config_validation as cv


def test_constant_payloads_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal send_packet payloads share one table; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("payload_tables.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    assert [v for _, v in tables] == [
        "{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}"
    ]
    assert main_cpp.count(f"set_data_static({tables[0][0]}, 8);") == 2
    assert "set_data_template(" in main_cpp


@pytest.mark.parametrize("data", [[0x01] * 255, "a" * 255])
def test_send_packet_accepts_255_bytes(data: list[int] | str) -> None:
    """The longest payload whose length fits the one-byte length field."""
    config = SEND_PACKET_ACTION_SCHEMA({"id": "transceiver", "data": data})
    assert len(config["data"]) == 255


@pytest.mark.parametrize("data", [[0x01] * 256, "a" * 256])
def test_send_packet_rejects_256_bytes(data: list[int] | str) -> None:
    """Payloads longer than the one-byte length field are rejected."""
    with pytest.raises(cv.Invalid, match="at most 255"):
        SEND_PACKET_ACTION_SCHEMA({"id": "transceiver", "data": data})
