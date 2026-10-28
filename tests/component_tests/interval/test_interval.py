"""Tests for the interval component."""

from collections.abc import Callable
from pathlib import Path


def test_interval_startup_delay_only_when_set(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A zero startup delay keeps the member default; a set delay is passed on."""
    main_cpp = generate_main(component_config_path("interval_test.yaml"))

    assert "delayed->set_startup_delay(5000);" in main_cpp
    assert main_cpp.count("set_startup_delay") == 1
