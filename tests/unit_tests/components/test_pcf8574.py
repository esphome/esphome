"""PCF8574 reset configuration and code generation."""

from unittest.mock import AsyncMock, Mock, patch

import pytest

from esphome.components import pcf8574
from esphome.const import CONF_ID, CONF_RESET


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
        patch.object(pcf8574.cg, "new_Pvariable", return_value=var),
        patch.object(pcf8574.cg, "register_component", new_callable=AsyncMock),
        patch.object(pcf8574.i2c, "register_i2c_device", new_callable=AsyncMock),
        patch.object(pcf8574.cg, "add"),
    ):
        await pcf8574.to_code(config)

    if expected:
        var.set_reset.assert_not_called()
    else:
        var.set_reset.assert_called_once_with(False)
