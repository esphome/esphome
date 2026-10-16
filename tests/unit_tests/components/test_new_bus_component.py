"""Tests for the new_<bus>_component helpers."""

from collections.abc import Callable
from typing import Any
from unittest.mock import AsyncMock, MagicMock, call, patch

import pytest

from esphome.components import i2c, spi, uart
from esphome.const import CONF_ID


@pytest.mark.parametrize(
    ("helper", "module", "register_name"),
    [
        (i2c.new_i2c_component, i2c, "register_i2c_device"),
        (uart.new_uart_component, uart, "register_uart_device"),
        (spi.new_spi_component, spi, "register_spi_device"),
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
        patch.object(module.cg, "new_Pvariable", order.new_Pvariable),
        patch.object(
            module.cg, "register_component", AsyncMock(side_effect=order.register)
        ),
        patch.object(module, register_name, AsyncMock(side_effect=order.bus)),
    ):
        result = await helper(config, 1, "two")

    assert result is var
    bus_call = (
        call.bus(var, config, write_only=False)
        if module is spi
        else call.bus(var, config)
    )
    assert order.mock_calls == [
        call.new_Pvariable("my_id", 1, "two"),
        call.register(var, config),
        bus_call,
    ]


@pytest.mark.asyncio
async def test_spi_passes_write_only() -> None:
    config = {CONF_ID: "my_id"}
    register = AsyncMock()
    with (
        patch.object(spi.cg, "new_Pvariable", MagicMock()),
        patch.object(spi.cg, "register_component", AsyncMock()),
        patch.object(spi, "register_spi_device", register),
    ):
        var = await spi.new_spi_component(config, write_only=True)

    register.assert_awaited_once_with(var, config, write_only=True)
