"""Tests for the new_<bus>_component helpers."""

from collections.abc import Callable
import inspect
from typing import Any
from unittest.mock import AsyncMock, MagicMock, call, patch

import pytest

from esphome import cpp_helpers
from esphome.components import ble_client, ble_device_base, i2c, modbus, spi, uart
from esphome.const import CONF_ID


@pytest.mark.parametrize(
    ("helper", "module", "register_name"),
    [
        (i2c.new_i2c_component, i2c, "register_i2c_device"),
        (uart.new_uart_component, uart, "register_uart_device"),
        (spi.new_spi_component, spi, "register_spi_device"),
        (
            ble_device_base.new_ble_device_component,
            ble_device_base,
            "register_ble_device",
        ),
        (ble_client.new_ble_node_component, ble_client, "register_ble_node"),
        (
            modbus.new_modbus_client_component,
            modbus,
            "register_modbus_client_device",
        ),
    ],
)
@pytest.mark.asyncio
async def test_creates_registers_and_attaches_in_order(
    helper: Callable[..., Any], module: Any, register_name: str
) -> None:
    config = {CONF_ID: "my_id"}
    var = MagicMock(name="var")
    order = MagicMock()
    order.new_Pvariable.return_value = var
    with (
        patch.object(cpp_helpers, "new_Pvariable", order.new_Pvariable),
        patch.object(
            cpp_helpers, "register_component", AsyncMock(side_effect=order.register)
        ),
        patch.object(module, register_name, AsyncMock(side_effect=order.bus)),
    ):
        result = await helper(config, 1, "two")

    assert result is var
    assert order.mock_calls == [
        call.new_Pvariable("my_id", 1, "two"),
        call.register(var, config),
        call.bus(var, config),
    ]


@pytest.mark.parametrize(
    "helper",
    [
        i2c.new_i2c_component,
        uart.new_uart_component,
        spi.new_spi_component,
        ble_device_base.new_ble_device_component,
        ble_client.new_ble_node_component,
        modbus.new_modbus_client_component,
    ],
)
def test_wrappers_are_plain_functions(helper: Callable[..., Any]) -> None:
    # An async wrapper would stay on the stack and become the component source.
    assert not inspect.iscoroutinefunction(helper)
