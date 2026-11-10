"""Tests for the SCD30 clock-stretch limit, set on its I2C bus."""

from collections.abc import Callable
from pathlib import Path


def test_esp8266_bus_gets_the_stretch_limit(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only the SCD30's bus is raised to 150 ms when its timeout is not set."""
    main_cpp = generate_main(component_config_path("esp8266_no_timeout.yaml"))
    assert "bus_a->set_timeout(150000);" in main_cpp
    assert "bus_b->set_timeout(" not in main_cpp


def test_esp8266_bus_keeps_a_configured_timeout(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A timeout the user set on the bus is kept."""
    main_cpp = generate_main(component_config_path("esp8266_timeout.yaml"))
    assert "bus_a->set_timeout(20000);" in main_cpp
    assert "set_timeout(150000)" not in main_cpp


def test_esp32_bus_is_unchanged(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only ESP8266 needs the longer stretch limit."""
    main_cpp = generate_main(component_config_path("esp32.yaml"))
    assert "set_timeout(" not in main_cpp
