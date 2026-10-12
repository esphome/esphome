"""Tests for abbwelcome constant data in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_constant_data_shares_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal data shares one table, empty or missing data is an empty table, lambdas stay templates."""
    main_cpp = generate_main(component_config_path("abbwelcome_payload_tables.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    assert [v for _, v in tables] == ["{0x10, 0x20, 0x30}"]
    assert main_cpp.count(f"set_data_static({tables[0][0]}, 3);") == 2
    assert main_cpp.count("set_data_static(nullptr, 0);") == 2
    assert main_cpp.count("set_data_template(") == 1
