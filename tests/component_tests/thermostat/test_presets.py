"""Tests for the thermostat preset codegen."""

from collections.abc import Callable
from pathlib import Path
import re


def test_presets_shared_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Thermostats with the same presets share one PROGMEM table per kind."""
    main_cpp = generate_main(component_config_path("presets.yaml"))

    standard = re.findall(
        r"static constexpr thermostat::ThermostatPresetEntry (\w+)\[\] PROGMEM",
        main_cpp,
    )
    custom = re.findall(
        r"static constexpr thermostat::ThermostatCustomPresetEntry (\w+)\[\] PROGMEM",
        main_cpp,
    )
    assert len(standard) == 1
    assert len(custom) == 1
    for var in ("thermo_a", "thermo_b"):
        assert f"{var}->set_preset_config({standard[0]}, 2);" in main_cpp
        assert f"{var}->set_custom_preset_config({custom[0]}, 1);" in main_cpp
    assert "climate::CLIMATE_MODE_HEAT" in main_cpp
