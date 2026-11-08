"""Tests for the multi click trigger codegen."""

from collections.abc import Callable
from pathlib import Path
import re


def test_multi_click_timing_tables(generate_main: Callable[[str | Path], str]) -> None:
    """Identical timings share one PROGMEM table; the default cooldown is not set."""
    main_cpp = generate_main(
        "tests/component_tests/binary_sensor/test_multi_click.yaml"
    )

    tables = re.findall(
        r"static constexpr binary_sensor::MultiClickTriggerEvent (\w+)\[\] PROGMEM",
        main_cpp,
    )
    assert len(tables) == 2
    single, long_press = tables
    triggers = re.findall(
        r"binary_sensor::MultiClickTrigger\((\w+), (\w+), (\d+)\);", main_cpp
    )
    assert triggers == [
        ("button_a", single, "2"),
        ("button_a", long_press, "1"),
        ("button_b", single, "2"),
    ]
    assert main_cpp.count("set_invalid_cooldown(") == 1
    assert "set_invalid_cooldown(500);" in main_cpp
