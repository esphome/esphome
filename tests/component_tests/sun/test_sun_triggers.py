"""Tests for the sun on_sunrise and on_sunset code generation."""

from collections.abc import Callable
from pathlib import Path


def test_triggers_share_one_poller(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every trigger filters the shared poller's callback on its own elevation and direction."""
    main_cpp = generate_main(component_config_path("triggers.yaml"))

    assert main_cpp.count(") sun::SunElevationPoller(sun_sun_id);") == 1
    assert main_cpp.count("add_on_elevation_callback(") == 3
    assert "sun::rose_past(last, current, -0.83333)" in main_cpp
    assert "sun::rose_past(last, current, 5.0)" in main_cpp
    assert "sun::set_past(last, current, -6.0)" in main_cpp
    assert "set_past(last, current, -0.83333)" not in main_cpp


def test_no_triggers_no_poller(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without on_sunrise or on_sunset nothing polls the elevation."""
    main_cpp = generate_main(component_config_path("no_triggers.yaml"))

    assert "SunElevationPoller" not in main_cpp
    assert "add_on_elevation_callback" not in main_cpp
