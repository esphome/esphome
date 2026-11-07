"""Event types are shared PROGMEM tables of type pointers."""

from collections.abc import Callable
from pathlib import Path
import re


def test_event_types_use_shared_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("event_types.yaml"))

    calls = {
        var: table
        for var, table, _ in re.findall(
            r"(\w+)->set_event_types_static\((\w+), (\d+)\);", main_cpp
        )
    }
    assert set(calls) == {"event_a", "event_b", "event_c"}
    assert calls["event_a"] == calls["event_b"] != calls["event_c"]
    assert (
        f'static constexpr const char * {calls["event_a"]}[] PROGMEM = {{"pressed", "held"}};'
        in main_cpp
    )
