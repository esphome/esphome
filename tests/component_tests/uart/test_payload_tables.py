"""Tests for UART constant payloads in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_constant_payloads_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal payloads share one table across switch, button and write action."""
    main_cpp = generate_main(component_config_path("payload_tables.yaml"))

    tables = dict(
        re.findall(
            r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
        )
    )
    shared = next(k for k, v in tables.items() if v == "{0x01, 0x02, 0x03}")
    assert sorted(tables.values()) == sorted(
        ["{0x01, 0x02, 0x03}", "{0x4F, 0x4E}", "{0x04}"]
    )
    assert f"shared_switch->set_data_on({shared}, 3);" in main_cpp
    assert f"shared_button->set_data({shared}, 3);" in main_cpp
    assert f"set_data_static({shared}, 3);" in main_cpp
    assert "set_data_static(nullptr, 0);" in main_cpp
    assert "set_data_template(" in main_cpp
