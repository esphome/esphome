"""Tests for canbus constant payloads in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.canbus import CANBUS_SEND_ACTION_SCHEMA
import esphome.config_validation as cv


def test_constant_payloads_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal send payloads share one table; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("payload_tables.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    assert [v for _, v in tables] == [
        "{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}"
    ]
    assert main_cpp.count(f"set_data_static({tables[0][0]}, 8);") == 2
    assert "set_data_template(" in main_cpp


@pytest.mark.parametrize("size", [8, 9])
@pytest.mark.parametrize("make", [lambda n: [0x01] * n, lambda n: "a" * n])
def test_send_one_frame_limit(
    size: int, make: Callable[[int], list[int] | str]
) -> None:
    """Payloads up to one 8 byte CAN frame are accepted, longer ones rejected."""
    config = {"canbus_id": "can_bus", "data": make(size)}
    if size <= 8:
        assert len(CANBUS_SEND_ACTION_SCHEMA(config)["data"]) == size
    else:
        with pytest.raises(cv.Invalid, match="at most 8"):
            CANBUS_SEND_ACTION_SCHEMA(config)
