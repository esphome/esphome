"""Tests for the on_multi_click codegen."""

from collections.abc import Callable
from pathlib import Path
import re

from tests.component_tests.helpers import function_body


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
    callbacks = re.findall(
        r"FireCallback<(\w+), binary_sensor::MultiClickBase>\((\w+), (\w+), (\d+)\);",
        main_cpp,
    )
    assert [args for _, *args in callbacks] == [
        ["button_a", single, "2"],
        ["button_a", long_press, "1"],
        ["button_b", single, "2"],
    ]
    # Each callback calls its own function, which runs the actions directly
    for (function, *_), message in zip(
        callbacks, ("a single", "a long", "b single"), strict=True
    ):
        body = function_body(main_cpp, function)
        assert f'"{message}"' in body
        assert "->trigger(" not in body
    assert main_cpp.count("set_invalid_cooldown(") == 1
    assert "set_invalid_cooldown(500);" in main_cpp
