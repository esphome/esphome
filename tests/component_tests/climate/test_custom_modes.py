"""Custom fan modes and presets from YAML live in shared flash tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_custom_modes_use_shared_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("custom_modes.yaml"))

    fan_tables = re.findall(
        r"static constexpr const char \* (climate_custom_fan_modes\w*)\[\] PROGMEM",
        main_cpp,
    )
    assert len(fan_tables) == 1  # Climate A and B share one table
    assert (
        main_cpp.count(f"set_supported_custom_fan_modes_static({fan_tables[0]}, 2);")
        == 2
    )
    assert (
        "climate_a->set_supported_custom_presets_static(climate_custom_presets, 1);"
        in main_cpp
    )
    assert "climate_c->set_supported_custom" not in main_cpp
