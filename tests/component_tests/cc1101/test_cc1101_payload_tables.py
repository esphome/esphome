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


@pytest.mark.parametrize("data", [[0x01] * 64, "a" * 64])
def test_send_packet_accepts_64_bytes(data: list[int] | str) -> None:
    """The longest payload that fits the TX FIFO."""
    config = SEND_PACKET_ACTION_SCHEMA({"id": "transceiver", "data": data})
    assert len(config["data"]) == 64


@pytest.mark.parametrize("data", [[0x01] * 65, "a" * 65])
def test_send_packet_rejects_65_bytes(data: list[int] | str) -> None:
    """Payloads longer than the TX FIFO are rejected."""
    with pytest.raises(cv.Invalid, match="at most 64"):
        SEND_PACKET_ACTION_SCHEMA({"id": "transceiver", "data": data})
