"""Tests for the new_<bus>_component helpers."""

from collections.abc import Callable
from typing import Any
from unittest.mock import AsyncMock, MagicMock, call, patch

import pytest

from esphome import cpp_helpers
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
