"""Tests for the ld2420 sensor, binary sensor and text sensor codegen."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_entities_attach_to_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("entities.yaml"))

    assert "radar->set_distance_sensor(" in main_cpp
    assert "radar->set_presence_binary_sensor(" in main_cpp
    assert "radar->set_fw_version_text_sensor(" in main_cpp
    assert "register_listener" not in main_cpp
    assert "LD2420Sensor" not in main_cpp
