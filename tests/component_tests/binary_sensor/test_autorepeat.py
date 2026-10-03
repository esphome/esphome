"""Tests for the binary sensor autorepeat filter codegen."""

from collections.abc import Callable
import re


def test_autorepeat_timings_shared_progmem_table(
    generate_main: Callable[[str], str],
) -> None:
    """Identical timings share one PROGMEM table; an empty autorepeat gets the defaults."""
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_autorepeat.yaml")

    tables = re.findall(
        r"static constexpr binary_sensor::AutorepeatFilterTiming (\w+)\[\] PROGMEM",
        main_cpp,
    )
    assert len(tables) == 2
    shared, default = tables
    assert main_cpp.count(f"AutorepeatFilter({shared}, 2);") == 2
    assert f"AutorepeatFilter({default}, 1);" in main_cpp
    default_table = main_cpp.split(f"{default}[] PROGMEM = ", 1)[1].split(";", 1)[0]
    for field in (".delay = 1000", ".time_off = 100", ".time_on = 900"):
        assert field in default_table
