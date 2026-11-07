"""Tests for the usb_uart component's driver source filtering."""

from collections.abc import Callable

from esphome.core import CORE


def test_only_configured_driver_compiled(
    generate_main: Callable[[str], str],
) -> None:
    generate_main("tests/component_tests/usb_uart/test_usb_uart_ft232.yaml")
    from esphome.components.usb_uart import FILTER_SOURCE_FILES

    defines = {define.name for define in CORE.defines}
    # cdc_acm is built into usb_uart.cpp and adds no driver define
    assert {d for d in defines if d.startswith("USE_USB_UART_")} == {
        "USE_USB_UART_FT23XX"
    }
    assert sorted(FILTER_SOURCE_FILES()) == ["ch34x.cpp", "cp210x.cpp", "pl2303.cpp"]
