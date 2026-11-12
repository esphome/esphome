"""Tests for the improv_serial transport selection."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_logger_port_requests_console(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without uart_id, Improv shares the logger's port through its console API."""
    generate_main(component_config_path("improv_logger_port.yaml"))

    defines = {d.name for d in CORE.defines}
    assert "USE_LOGGER_CONSOLE" in defines
    assert "USE_IMPROV_SERIAL_UART" not in defines


def test_dedicated_uart_skips_console(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A dedicated UART leaves the logger's console API out."""
    generate_main(component_config_path("improv_dedicated_uart.yaml"))

    defines = {d.name for d in CORE.defines}
    assert "USE_IMPROV_SERIAL_UART" in defines
    assert "USE_LOGGER_CONSOLE" not in defines
