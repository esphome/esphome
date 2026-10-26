"""Tests for the logger component's lazy platform tables."""

import subprocess
import sys

from esphome.components import logger


def test_uart_selection_tables_exposed_for_external_tooling() -> None:
    """device-builder introspects these names off the module; keep them
    accessible (and discoverable) and stable between accesses."""
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


def test_dir_does_not_import_platform_packages() -> None:
    """Listing the names must not load the platform packages; only reading a
    table may. Checked in a fresh interpreter so nothing loaded by other tests
    hides a leak, and nothing here changes what later tests see."""
    check = (
        "import sys; from esphome.components import logger; dir(logger); "
        "print(','.join(m for m in ('esphome.components.esp32', "
        "'esphome.components.libretiny', 'esphome.components.zephyr') "
        "if m in sys.modules))"
    )
    result = subprocess.run(
        [sys.executable, "-c", check], capture_output=True, text=True, check=True
    )
    assert result.stdout.strip() == ""
