"""Tests for compiling the RP2040 hardware UART only when a uart: entry exists."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

RP2_SOURCE = "uart_component_rp2.cpp"


@pytest.mark.parametrize(
    ("config", "hardware"),
    [("wake_on_rx_rp2.yaml", True), ("virtual_only_rp2.yaml", False)],
    ids=["uart_entry", "virtual_only"],
)
def test_rp2_hardware_source_only_with_uart_entry(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    hardware: bool,
) -> None:
    """Naming Serial1/Serial2 links arduino-pico's SerialUART objects, so the file is left out without a uart: entry."""
    generate_main(component_config_path(config))
    from esphome.components.uart import FILTER_SOURCE_FILES

    assert ("USE_UART_HARDWARE" in {d.name for d in CORE.defines}) is hardware
    assert (RP2_SOURCE in FILTER_SOURCE_FILES()) is not hardware
