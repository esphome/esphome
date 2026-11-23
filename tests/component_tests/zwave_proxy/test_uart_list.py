"""Tests for the zwave_proxy check on a list of UARTs."""

import pytest

from esphome import config_validation as cv
from esphome.components.zwave_proxy import final_validate
from esphome.const import CONF_CHANNELS, CONF_ID, CONF_UART_ID
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.helpers import uart_full_config


def _validate(uart_ids: list[str]) -> None:
    usb_uart: list[ConfigType] = [
        {CONF_CHANNELS: [{CONF_ID: ID("usb_0")}]},
        {CONF_CHANNELS: [{CONF_ID: ID("usb_1")}]},
    ]
    fv.full_config.set(uart_full_config(usb_uart=usb_uart))
    final_validate({CONF_UART_ID: [ID(uart_id) for uart_id in uart_ids]})


@pytest.mark.parametrize(
    "uart_ids",
    [["uart_0"], ["usb_0"], ["usb_0", "usb_1"]],
)
def test_accepts_one_uart_or_only_usb_channels(uart_ids: list[str]) -> None:
    _validate(uart_ids)


@pytest.mark.parametrize(
    "uart_ids",
    [["uart_0", "uart_1"], ["uart_0", "usb_0"], ["usb_0", "usb_1", "uart_0"]],
)
def test_rejects_a_list_with_a_fixed_uart(uart_ids: list[str]) -> None:
    with pytest.raises(cv.Invalid, match="must be a usb_uart channel"):
        _validate(uart_ids)
