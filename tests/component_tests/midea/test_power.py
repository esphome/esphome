"""The optional Group 7 sensor must remain distinct from legacy power telemetry."""

from collections.abc import Callable
from pathlib import Path


def test_power_sensors_are_distinct(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("power.yaml"))
    assert "tested_ac->set_compressor_power_sensor(compressor);" in main_cpp
    assert "tested_ac->set_power_sensor(legacy_power);" in main_cpp
    assert main_cpp.count("set_compressor_power_sensor(") == 1


def test_group7_is_opt_in(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("without_power.yaml"))
    assert "set_compressor_power_sensor(" not in main_cpp
