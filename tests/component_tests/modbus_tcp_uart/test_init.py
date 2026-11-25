"""Tests for the modbus_tcp_uart schema."""

from esphome.components import modbus_tcp_uart, uart
from esphome.const import CONF_ID


def test_devices_are_checked_against_the_tcp_uart_settings() -> None:
    config = modbus_tcp_uart.CONFIG_SCHEMA(
        {CONF_ID: "adapter", modbus_tcp_uart.CONF_TCP_UART_ID: "link"}
    )
    # A device such as senseair on the adapter is checked against the settings of the tcp_uart.
    assert str(uart._settings_source(config[CONF_ID])) == "link"
