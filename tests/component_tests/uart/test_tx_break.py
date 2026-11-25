"""Tests for the request of UARTComponent::set_tx_break()."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_tx_break_is_not_compiled_in_by_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("parity_default.yaml"))

    assert "USE_UART_TX_BREAK" not in {d.name for d in CORE.defines}
