"""Fan preset modes from YAML live in shared flash tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_preset_modes_use_shared_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("preset_modes.yaml"))

    tables = re.findall(
        r"static constexpr const char \* (fan_preset_modes\w*)\[\] PROGMEM", main_cpp
    )
    assert len(tables) == 1  # Fan A and B share one table
    assert main_cpp.count(f"set_supported_preset_modes_static({tables[0]}, 2);") == 2
    assert "fan_c->set_supported_preset_modes" not in main_cpp
