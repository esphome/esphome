"""Tests for the ld2450 link timeout option."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_default_link_timeout_reaches_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("link_default.yaml"))

    assert "radar->set_link_timeout(5000);" in main_cpp
    assert "set_connected_binary_sensor" not in main_cpp
