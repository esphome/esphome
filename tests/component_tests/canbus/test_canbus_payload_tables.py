"""Tests for canbus constant payloads in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
import yaml

from esphome.config import read_config
from esphome.core import CORE


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
    for setter in (
        "set_default_fd",
        "set_default_bit_rate_switch",
        "set_fd",
        "set_bit_rate_switch",
        "set_pad_data",
    ):
        assert f"->{setter}(" not in main_cpp


@pytest.mark.parametrize("size", [8, 9])
@pytest.mark.parametrize("make", [lambda n: [0x01] * n, lambda n: "a" * n])
def test_send_one_frame_limit(
    size: int,
    make: Callable[[int], list[int] | str],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Final validation limits classic CAN payloads to eight bytes."""
    config = {
        "esphome": {
            "name": "test",
            "on_boot": [{"then": [{"canbus.send": {"data": make(size)}}]}],
        },
        "esp32": {"board": "esp32dev"},
        "canbus": [
            {
                "platform": "esp32_can",
                "id": "can_bus",
                "can_id": 4,
                "rx_pin": 4,
                "tx_pin": 5,
            }
        ],
    }
    path = tmp_path / "test.yaml"
    path.write_text(yaml.safe_dump(config), encoding="utf-8")
    CORE.config_path = path
    result = read_config({})
    output = capsys.readouterr().out
    assert (result is not None) is (size <= 8), output
    if size > 8:
        assert "Classic CAN data must be at most 8 bytes" in output
        assert "canbus.send" in output
