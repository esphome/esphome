"""Tests for the uart helpers shared by components that own a UART."""

from esphome.components.uart import subtree_references_uart
from esphome.core import ID


def test_finds_a_nested_uart_id() -> None:
    config = {"modbus": [{"id": ID("hub"), "uart_id": ID("bus")}]}
    assert subtree_references_uart(config, "bus")


def test_ignores_other_uarts_and_other_keys() -> None:
    config = {"modbus": [{"uart_id": ID("other")}], "sensor": [{"id": ID("bus")}]}
    assert not subtree_references_uart(config, "bus")


def test_finds_another_key() -> None:
    config = {"modbus_tcp_uart": [{"tcp_uart_id": ID("bus")}]}
    assert subtree_references_uart(config, "bus", "tcp_uart_id")
    assert not subtree_references_uart(config, "bus")


def test_handles_scalars() -> None:
    assert not subtree_references_uart("bus", "bus")
