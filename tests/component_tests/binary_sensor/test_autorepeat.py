"""Tests for the binary sensor autorepeat filter codegen."""

from collections.abc import Callable
import re


def test_autorepeat_timings_shared_progmem_table(
    generate_main: Callable[[str], str],
) -> None:
    """Filters with identical timings share one PROGMEM table; nothing is copied."""
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_autorepeat.yaml")

    tables = re.findall(
        r"static constexpr binary_sensor::AutorepeatFilterTiming (\w+)\[\] PROGMEM",
        main_cpp,
    )
    assert len(tables) == 2
    shared, default = tables
    assert f"AutorepeatFilter({shared}, 2);" in main_cpp
    assert main_cpp.count(f"AutorepeatFilter({shared}, 2);") == 2
    assert f"AutorepeatFilter({default}, 1);" in main_cpp
    assert "AutorepeatFilter<" not in main_cpp


def test_autorepeat_default_timing(generate_main: Callable[[str], str]) -> None:
    """An empty autorepeat uses the documented default timing."""
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_autorepeat.yaml")

    default = re.search(
        r"AutorepeatFilterTiming (\w+)\[\] PROGMEM = \{binary_sensor::AutorepeatFilterTiming\{\n"
        r"  \.delay = 1000,\n  \.time_off = 100,\n  \.time_on = 900,\n\}\};",
        main_cpp,
    )
    assert default is not None
    assert f"AutorepeatFilter({default.group(1)}, 1);" in main_cpp
