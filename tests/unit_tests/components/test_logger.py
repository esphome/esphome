"""Tests for the logger component's lazy platform tables."""

import sys


def test_uart_selection_tables_exposed_for_external_tooling() -> None:
    """device-builder introspects these names off the module; keep them
    accessible (and discoverable) without importing the platform packages."""
    for mod in ("esphome.components.esp32", "esphome.components.libretiny"):
        sys.modules.pop(mod, None)

    from esphome.components import logger

    assert "UART_SELECTION_ESP32" in dir(logger)
    assert "UART_SELECTION_LIBRETINY" in dir(logger)
    assert logger.UART_SELECTION_ESP32["ESP32C3"] == [
        "UART0",
        "UART1",
        "USB_CDC",
        "USB_SERIAL_JTAG",
    ]
    assert "bk72xx" in logger.UART_SELECTION_LIBRETINY
    # Repeated access returns the same cached object
    assert logger.UART_SELECTION_ESP32 is logger.UART_SELECTION_ESP32
