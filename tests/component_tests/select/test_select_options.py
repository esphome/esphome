"""Select options are shared PROGMEM tables of option pointers."""

from collections.abc import Callable
from pathlib import Path
import re


def test_select_options_use_shared_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("select_options.yaml"))

    calls = {
        var: table
        for var, table, _ in re.findall(
            r"(\w+)->traits\.set_options_static\((\w+), (\d+)\);", main_cpp
        )
    }
    assert set(calls) == {"fan_a", "fan_b", "mode"}
    assert calls["fan_a"] == calls["fan_b"] != calls["mode"]
    assert (
        f'static constexpr const char * {calls["fan_a"]}[] PROGMEM = {{"low", "medium", "high"}};'
        in main_cpp
    )
    # The copy select takes its options from the source at setup
    assert "fan_copy->traits.set_options_static(" not in main_cpp
