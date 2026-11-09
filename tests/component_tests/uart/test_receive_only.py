"""Tests for UARTs that drop what is written to them."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_UART_ID, PlatformFramework
from esphome.core import ID
from tests.component_tests.helpers import uart_full_config
from tests.component_tests.types import SetCoreConfigCallable


def _device(uart_id: str, **requirements: bool) -> dict:
    return uart.final_validate_device_schema("test_device", **requirements)(
        {CONF_UART_ID: ID(uart_id)}
    )


def test_device_that_transmits_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A device that requires TX cannot use a receive-only UART."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: ID("tap")}]),
    )
    uart.mark_receive_only(ID("tap"))
    with pytest.raises(
        cv.Invalid,
        match="test_device requires the uart referenced by uart_id to transmit, but it is receive-only",
    ):
        _device("tap", require_tx=True)


def test_device_that_only_receives_is_valid(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A device that only reads can use a receive-only UART."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: ID("tap")}]),
    )
    uart.mark_receive_only(ID("tap"))
    _device("tap", require_rx=True)


def test_unmarked_uart_lets_a_device_transmit(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Without the mark, the same device that requires TX is accepted."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: ID("tap")}]),
    )
    _device("tap", require_tx=True)


def test_only_the_marked_uart_is_receive_only(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A device that requires TX can use bus while tap next to it is receive-only."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: ID("bus")}, {CONF_ID: ID("tap")}]),
    )
    uart.mark_receive_only(ID("tap"))
    _device("bus", require_tx=True)


def test_a_generated_id_is_found_once_named(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """CONFIG_SCHEMA marks a generated id before the ID pass gives it its name."""
    generated = ID(None, is_declaration=True, type=uart.UARTComponent)
    uart.mark_receive_only(generated)
    name = generated.resolve([])
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: generated}]),
    )
    with pytest.raises(cv.Invalid, match="but it is receive-only"):
        _device(name, require_tx=True)
