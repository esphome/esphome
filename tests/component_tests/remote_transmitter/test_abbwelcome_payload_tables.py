"""Tests for abbwelcome constant data in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_constant_data_shares_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal data shares one table, empty data is a null table, lambdas are called inline."""
    main_cpp = generate_main(component_config_path("abbwelcome_payload_tables.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (\w+)\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    assert [v for _, v in tables] == ["{0x10, 0x20, 0x30}"]
    assert main_cpp.count(f"apply_call.data.set_data({tables[0][0]}, 3);") == 2
    # Empty data is a null table; missing data leaves the default empty payload
    assert main_cpp.count("apply_call.data.set_data(nullptr, 0);") == 1
    assert (
        main_cpp.count("apply_call.data.set_data([]() -> std::vector<uint8_t> {") == 1
    )
    assert "std::vector<uint8_t>{" not in main_cpp
