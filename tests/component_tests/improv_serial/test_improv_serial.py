"""Tests for the improv_serial transport selection."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "console", "uart"),
    [
        # Without uart_id, Improv shares the logger's port through its console API
        pytest.param("improv_logger_port.yaml", True, False, id="logger_port"),
        # A dedicated UART leaves the logger's console API out
        pytest.param("improv_dedicated_uart.yaml", False, True, id="dedicated_uart"),
        # The Zephyr logger has no console, so Improv there is a no-op
        pytest.param("improv_nrf52.yaml", False, False, id="nrf52"),
    ],
)
def test_transport_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    console: bool,
    uart: bool,
) -> None:
    """Each transport compiles in only the code it uses."""
    generate_main(component_config_path(config))

    defines = {d.name for d in CORE.defines}
    assert ("USE_LOGGER_CONSOLE" in defines) is console
    assert ("USE_IMPROV_SERIAL_UART" in defines) is uart
