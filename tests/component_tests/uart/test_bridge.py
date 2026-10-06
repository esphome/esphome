"""Tests for the bridge uart platform's final validation and code generation."""

from unittest import mock

import pytest

from esphome import config_validation as cv
from esphome.components import uart
from esphome.components.uart import bridge
from esphome.components.uart.bridge import CONF_PEER_ID
from esphome.config import Config
from esphome.const import (
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

_final_validate = bridge._final_validate


def _full_config(uarts: list[ConfigType] | None = None, **domains) -> Config:
    """A full config declaring uart_0, uart_1 and uart_2 as the ID pass leaves it."""
    uarts = uarts or [{CONF_ID: ID(f"uart_{index}")} for index in range(3)]
    full = Config()
    full["uart"] = uarts
    for index, uart_conf in enumerate(uarts):
        full.declare_ids.append((uart_conf[CONF_ID], ["uart", index, CONF_ID]))
    full.update(domains)
    return full


def _bridge_config(uart_id: str, peer_id: str) -> ConfigType:
    return {CONF_UART_ID: ID(uart_id), CONF_PEER_ID: ID(peer_id)}


def _set_config(set_core_config: SetCoreConfigCallable, full: Config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)


def test_accepts_two_free_uarts(set_core_config: SetCoreConfigCallable) -> None:
    _set_config(set_core_config, _full_config())
    _final_validate(_bridge_config("uart_0", "uart_1"))


def test_rejects_the_same_uart_on_both_ends(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_config(set_core_config, _full_config())
    with pytest.raises(cv.Invalid, match="same UART"):
        _final_validate(_bridge_config("uart_0", "uart_0"))


def test_rejects_a_uart_in_two_bridges_under_either_key(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_config(set_core_config, _full_config())
    _final_validate(_bridge_config("uart_0", "uart_1"))
    with pytest.raises(cv.Invalid, match="already bridged"):
        _final_validate(_bridge_config("uart_2", "uart_0"))


def test_rejects_a_uart_another_component_uses(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_config(
        set_core_config,
        _full_config(modbus=[{CONF_ID: ID("hub"), CONF_UART_ID: ID("uart_1")}]),
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "uart_1"))


def test_rejects_dummy_receiver_on_an_end(
    set_core_config: SetCoreConfigCallable,
) -> None:
    uarts = [
        {CONF_ID: ID("uart_0")},
        {CONF_ID: ID("uart_1"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: True}},
    ]
    _set_config(set_core_config, _full_config(uarts))
    with pytest.raises(cv.Invalid, match="dummy_receiver"):
        _final_validate(_bridge_config("uart_0", "uart_1"))


def test_allows_debug_without_dummy_receiver(
    set_core_config: SetCoreConfigCallable,
) -> None:
    uarts = [
        {CONF_ID: ID("uart_0"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: False}},
        {CONF_ID: ID("uart_1")},
    ]
    _set_config(set_core_config, _full_config(uarts))
    _final_validate(_bridge_config("uart_0", "uart_1"))


_HARDWARE = uart.IDFUARTComponent
_HOST = uart.HostUartComponent
_VIRTUAL = uart.uart_ns.class_("TestVirtualUart", uart.VirtualUARTComponent)
# Any other UART, e.g. a channel of a UART expander.
_OTHER = uart.uart_ns.class_("TestOtherUart", uart.UARTComponent)
# A UART that marks its id with uart.mark_unclocked(), e.g. tcp_uart or a usb_uart channel.
_UNCLOCKED = uart.uart_ns.class_("TestUnclockedUart", uart.UARTComponent)
_SETTERS = ("set_virtual_a", "set_virtual_b", "set_wire_a", "set_wire_b")


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("a_type", "b_type", "expected"),
    [
        (_HARDWARE, _HARDWARE, ["set_wire_a", "set_wire_b"]),
        (_VIRTUAL, _HARDWARE, ["set_virtual_a", "set_wire_b"]),
        (_HARDWARE, _VIRTUAL, ["set_virtual_b", "set_wire_a"]),
        (_VIRTUAL, _VIRTUAL, ["set_virtual_a", "set_virtual_b"]),
        (_HOST, _OTHER, ["set_wire_a", "set_wire_b"]),
        (_OTHER, _VIRTUAL, ["set_virtual_b", "set_wire_a"]),
        (_UNCLOCKED, _HARDWARE, ["set_wire_b"]),
        (_OTHER, _UNCLOCKED, ["set_wire_a"]),
        (_UNCLOCKED, _VIRTUAL, ["set_virtual_b"]),
    ],
)
async def test_to_code_tells_the_bridge_what_each_end_is(
    a_type: object, b_type: object, expected: list[str]
) -> None:
    ends = {
        "end_a": ID("end_a", is_declaration=True, type=a_type),
        "end_b": ID("end_b", is_declaration=True, type=b_type),
    }
    for end in ends.values():
        if end.type is _UNCLOCKED:
            uart.mark_unclocked(end)

    async def full_id(id_: ID) -> tuple[ID, mock.MagicMock]:
        return ends[id_.id], mock.MagicMock(name=id_.id)

    var = mock.MagicMock()
    with (
        mock.patch.object(bridge.cg, "get_variable_with_full_id", side_effect=full_id),
        mock.patch.object(bridge.cg, "new_Pvariable", return_value=var),
        mock.patch.object(bridge.cg, "register_component", new_callable=mock.AsyncMock),
        mock.patch.object(bridge.cg, "add"),
    ):
        await bridge.to_code(
            {CONF_ID: ID("link"), CONF_UART_ID: ID("end_a"), CONF_PEER_ID: ID("end_b")}
        )
    called = [name for name in _SETTERS if getattr(var, name).called]
    assert called == expected
