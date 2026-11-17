"""Tests for the usb_host sdkconfig defaults."""

from collections.abc import Callable

from esphome.components.esp32 import RawSdkconfigValue
from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.core import CORE


def test_usb_host_uses_default_control_transfer_size(
    generate_main: Callable[[str], str],
) -> None:
    """usb_host retains its existing 1024-byte default."""
    generate_main("tests/component_tests/usb_host/usb_host.yaml")

    assert (
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][
            "CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE"
        ]
        == 1024
    )


def test_usb_host_respects_explicit_control_transfer_size(
    generate_main: Callable[[str], str],
) -> None:
    """An explicit sdkconfig option takes precedence over the component default."""
    generate_main(
        "tests/component_tests/usb_host/usb_host_control_transfer_override.yaml"
    )

    value = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][
        "CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE"
    ]
    assert isinstance(value, RawSdkconfigValue)
    assert value.value == "4096"
