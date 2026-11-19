"""Tests for the logger component's lazy platform tables."""

import subprocess
import sys

import pytest

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


@pytest.mark.parametrize(
    ("variant", "label"),
    [
        ("NRF54L15", "uart20"),
        ("NRF54LM20", "uart20"),
        ("NRF52840", "uart0"),
        ("NRF52832", "uart0"),
    ],
)
def test_zephyr_uart0_label(variant: str, label: str) -> None:
    from esphome.components.logger import _zephyr_uart0_label
    from esphome.components.nrf52.framework import set_nrf52_variant

    set_nrf52_variant(variant)
    assert _zephyr_uart0_label() == label


@pytest.mark.parametrize("variant", ["NRF52832", "NRF54L15"])
def test_variants_without_usb_reject_usb_cdc(variant: str) -> None:
    from esphome.components.logger import uart_selection
    from esphome.components.nrf52.framework import set_nrf52_variant
    import esphome.config_validation as cv
    from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_NRF52
    from esphome.core import CORE

    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    set_nrf52_variant(variant)
    assert uart_selection("uart0") == "UART0"
    with pytest.raises(cv.Invalid):
        uart_selection("USB_CDC")


@pytest.mark.parametrize(
    ("board", "given", "expected"),
    [
        ("xiao_nrf54lm20a", {}, {"hardware_uart": "UART0"}),
        ("xiao_nrf54lm20a", {"hardware_uart": "USB_CDC"}, {"hardware_uart": "USB_CDC"}),
        ("nrf54lm20dk/nrf54lm20a/cpuapp", {}, {}),
    ],
)
def test_board_with_uart_console_defaults_to_uart0(
    board: str, given: dict[str, str], expected: dict[str, str]
) -> None:
    """The XIAO nRF54LM20A's USB port is its debugger's UART bridge; a user choice is kept."""
    from esphome.components.logger import _nrf52_board_default_uart
    from esphome.components.nrf52.framework import set_nrf52_board
    from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_NRF52
    from esphome.core import CORE

    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    set_nrf52_board(board)
    assert _nrf52_board_default_uart(given) == expected
