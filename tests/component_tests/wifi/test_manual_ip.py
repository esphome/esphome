"""Tests for the manual IP table wifi codegen emits (MANUAL_IP_TABLE in wifi_component.h)."""

from collections.abc import Callable
from pathlib import Path
import re


def test_manual_ip_rows_are_shared(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Networks with the same settings share a row; the AP row packs its missing DNS as zeros."""
    main_cpp = generate_main(component_config_path("manual_ip.yaml"))

    own = "192, 168, 1, 10, 192, 168, 1, 1, 255, 255, 255, 0, 1, 1, 1, 1, 8, 8, 8, 8"
    inherited = (
        "192, 168, 2, 20, 192, 168, 2, 1, 255, 255, 255, 0, 192, 168, 2, 1, 0, 0, 0, 0"
    )
    ap = "192, 168, 4, 1, 192, 168, 4, 1, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0"
    assert (
        f"esphome::wifi::MANUAL_IP_TABLE[] PROGMEM = {{{own}, {inherited}, {ap}}};"
        in main_cpp
    )
    # Three networks in config order, then the AP
    assert re.findall(r"set_manual_ip\((\d+)\)", main_cpp) == ["0", "1", "1", "2"]
