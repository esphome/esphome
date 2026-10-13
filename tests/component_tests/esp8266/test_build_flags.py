"""Tests for ESP8266 build flags."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_esp8266_keeps_switches_out_of_ram(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("minimal.yaml"))

    assert "-fno-tree-switch-conversion" in CORE.build_flags
