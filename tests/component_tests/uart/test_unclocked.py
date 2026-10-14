"""Tests for marking UARTs whose bytes do not arrive with the timing of a serial line."""

from esphome.components import ble_nus, tcp_uart, uart, usb_cdc_acm
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable


def test_only_a_marked_uart_is_unclocked() -> None:
    marked = ID("link", is_declaration=True)
    assert uart.mark_unclocked(marked) is marked
    assert uart.is_unclocked(ID("link"))
    assert not uart.is_unclocked(ID("bus"))


def test_a_generated_id_is_found_once_named() -> None:
    generated = uart.mark_unclocked(
        ID(None, is_declaration=True, type=uart.UARTComponent)
    )
    name = generated.resolve([])
    assert uart.is_unclocked(ID(name))


def test_tcp_uart_is_unclocked() -> None:
    tcp_uart.CONFIG_SCHEMA({CONF_ID: "link", "role": "server", "port": 6638})
    assert uart.is_unclocked(ID("link"))


def test_every_usb_uart_channel_is_unclocked(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={"variant": "ESP32S3"})
    # Its schema is built on import and needs the target.
    from esphome.components import usb_uart

    schema = usb_uart.channel_schema(usb_uart._TYPES_BY_NAME["FT2232"])
    schema(
        {
            "channels": [
                {CONF_ID: "ch_a", "baud_rate": 9600},
                {CONF_ID: "ch_b", "baud_rate": 115200},
            ]
        }
    )
    assert uart.is_unclocked(ID("ch_a"))
    assert uart.is_unclocked(ID("ch_b"))


def test_usb_cdc_acm_interface_is_unclocked() -> None:
    usb_cdc_acm.INTERFACE_SCHEMA({CONF_ID: "cdc"})
    assert uart.is_unclocked(ID("cdc"))


def test_ble_nus_is_unclocked(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    ble_nus.CONFIG_SCHEMA({CONF_ID: "nus", "type": "uart"})
    assert uart.is_unclocked(ID("nus"))
