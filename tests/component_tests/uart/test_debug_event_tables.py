"""uart debug delimiters and uart event patterns live in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_debug_delimiter_and_event_matchers_use_shared_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("debug_event_tables.yaml"))

    # "\r\n" and [0x0D, 0x0A] are the same bytes, so both debuggers share one table
    delimiters = re.findall(r"->set_after_delimiter\((\w+), 2\);", main_cpp)
    assert len(delimiters) == 2
    assert delimiters[0] == delimiters[1]
    assert "add_delimiter_byte" not in main_cpp

    # Both events have the same patterns, so they share the byte tables and the matcher table
    patterns = re.findall(r"static constexpr uint8_t uart_event_match\w*\[\]", main_cpp)
    assert len(patterns) == 2
    calls = re.findall(r"->set_matchers\((\w+), 2, 3\);", main_cpp)
    assert len(calls) == 2
    assert calls[0] == calls[1]
    assert main_cpp.count("static constexpr uart::UARTEventMatcher") == 1
    assert "add_event_matcher" not in main_cpp
