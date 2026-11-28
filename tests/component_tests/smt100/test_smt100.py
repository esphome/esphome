"""Tests for the icons of the smt100 sensors."""

from collections.abc import Callable
from pathlib import Path
import re

CONFIG = "tests/component_tests/smt100/test_smt100.yaml"

# Entity id to the icon the generated code gives it. Temperature and voltage have none: their device
# class provides it.
EXPECTED_ICONS = {
    "default_counts": "mdi:counter",
    "default_permittivity": "mdi:sine-wave",
    "default_moisture": "mdi:water-percent",
    "default_temperature": None,
    "default_voltage": None,
    "override_counts": "mdi:numeric",
    "override_permittivity": "mdi:alpha-e",
}


def test_sensor_icons(generate_main: Callable[[str | Path], str]) -> None:
    """Counts and permittivity get a default icon, and an icon set in the YAML replaces it."""
    main_cpp = generate_main(CONFIG)

    for entity, icon in EXPECTED_ICONS.items():
        # configure_sensor(...) ends with a comment that lists the entity's metadata
        match = re.search(rf"configure_sensor\({entity}, [^\n]*\);([^\n]*)", main_cpp)
        assert match, f"{entity} is not configured"
        metadata = match.group(1)
        if icon is None:
            assert "icon:" not in metadata, entity
        else:
            assert f"icon:{icon}" in metadata, entity
