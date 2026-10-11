"""Tests for ESP-NOW constant send payloads in shared flash tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_constant_payloads_share_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal payloads share one table; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("payload_tables.yaml"))

    tables = dict(
        re.findall(
            r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
        )
    )
    assert sorted(tables.values()) == sorted(["{0x01, 0x02, 0x03}", "{0x4F, 0x4B}"])
    shared = next(k for k, v in tables.items() if v == "{0x01, 0x02, 0x03}")
    assert main_cpp.count(f"set_data_static({shared}, 3);") == 2
    assert "set_data_template(" in main_cpp
