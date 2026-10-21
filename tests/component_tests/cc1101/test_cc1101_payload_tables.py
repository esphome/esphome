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


@pytest.mark.parametrize("size", [64, 65])
@pytest.mark.parametrize("make", [lambda n: [0x01] * n, lambda n: "a" * n])
def test_send_packet_tx_fifo_limit(
    size: int, make: Callable[[int], list[int] | str]
) -> None:
    """Payloads up to the 64 byte TX FIFO are accepted, longer ones rejected."""
    config = {"id": "transceiver", "data": make(size)}
    if size <= 64:
        assert len(SEND_PACKET_ACTION_SCHEMA(config)["data"]) == size
    else:
        with pytest.raises(cv.Invalid, match="at most 64"):
            SEND_PACKET_ACTION_SCHEMA(config)
