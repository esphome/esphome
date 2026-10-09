"""Tests for the virtual UART base's source filtering and require function."""

from unittest.mock import patch

from esphome.components import uart
from esphome.core import Define


def test_virtual_uart_filtered_until_required() -> None:
    """uart_virtual.cpp compiles only when a component asks for it."""
    with patch("esphome.config_helpers.CORE") as mock_core:
        mock_core.defines = set()
        assert "uart_virtual.cpp" in uart._define_filter()

        mock_core.defines = {Define("USE_UART_VIRTUAL")}
        assert "uart_virtual.cpp" not in uart._define_filter()


def test_require_virtual_uart_adds_the_define() -> None:
    with patch.object(uart.cg, "add_define") as add_define:
        uart.require_virtual_uart()
    add_define.assert_called_once_with("USE_UART_VIRTUAL")
