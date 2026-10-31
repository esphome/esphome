"""PCF8574 reset configuration and code generation."""

from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome.components import pcf8574
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_RESET,
)


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "options, expected",
    [
        ({}, True),
        ({"reset": True}, True),
        ({"reset": False}, False),
    ],
)
async def test_reset_configuration(options: dict, expected: bool) -> None:
    config = pcf8574.CONFIG_SCHEMA({CONF_ID: "hub", **options})
    assert config[CONF_RESET] is expected
    var = Mock()
    with (
        patch.object(pcf8574.i2c, "new_i2c_component", AsyncMock(return_value=var)),
        patch.object(pcf8574.cg, "add"),
    ):
        await pcf8574.to_code(config)

    if expected:
        var.set_reset.assert_not_called()
    else:
        var.set_reset.assert_called_once_with(False)


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "number, is_input",
    [
        (0, True),
        (15, True),
        (0, False),
        (15, False),
    ],
)
async def test_pin_registers_inputs_only(number: int, is_input: bool) -> None:
    config = {
        CONF_NUMBER: number,
        CONF_MODE: {CONF_INPUT: is_input, CONF_OUTPUT: not is_input},
        pcf8574.CONF_PCF8574: "hub",
    }
    parent = Mock()
    with (
        patch.object(pcf8574.pins, "new_parented_pin", new_callable=AsyncMock),
        patch.object(pcf8574.cg, "get_variable", AsyncMock(return_value=parent)),
        patch.object(pcf8574.cg, "add"),
    ):
        await pcf8574.pcf8574_pin_to_code(config)

    if is_input:
        parent.register_input_pin.assert_called_once_with(number)
    else:
        parent.register_input_pin.assert_not_called()
