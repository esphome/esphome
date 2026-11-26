"""Tests for the zwave_proxy uart_id option, given as one UART or a list."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.zwave_proxy import CONFIG_SCHEMA, final_validate
from esphome.config import read_config
from esphome.const import CONF_CHANNELS, CONF_ID, CONF_UART_ID
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.types import ConfigType
from tests.component_tests.helpers import uart_full_config


def _validate(uart_ids: list[str] | str) -> None:
    usb_uart: list[ConfigType] = [
        {CONF_CHANNELS: [{CONF_ID: ID("usb_0")}]},
        {CONF_CHANNELS: [{CONF_ID: ID("usb_1")}]},
    ]
    fv.full_config.set(uart_full_config(usb_uart=usb_uart))
    if isinstance(uart_ids, str):
        final_validate({CONF_UART_ID: ID(uart_ids)})
    else:
        final_validate({CONF_UART_ID: [ID(uart_id) for uart_id in uart_ids]})


@pytest.mark.parametrize(
    "uart_ids",
    ["uart_0", ["uart_0"], ["usb_0"], ["usb_0", "usb_1"]],
)
def test_accepts_one_uart_or_only_usb_channels(uart_ids: list[str] | str) -> None:
    _validate(uart_ids)


@pytest.mark.parametrize(
    "uart_ids",
    [["uart_0", "uart_1"], ["uart_0", "usb_0"], ["usb_0", "usb_1", "uart_0"]],
)
def test_rejects_a_list_with_a_fixed_uart(uart_ids: list[str]) -> None:
    with pytest.raises(cv.Invalid, match="must be a usb_uart channel"):
        _validate(uart_ids)


@pytest.mark.parametrize(
    ("config", "uart_ids"),
    [
        ({CONF_UART_ID: "uart_0"}, ["uart_0"]),
        ({CONF_UART_ID: ["uart_0"]}, ["uart_0"]),
        ({CONF_UART_ID: ["usb_1", "usb_0"]}, ["usb_1", "usb_0"]),
    ],
)
def test_uart_id_becomes_a_list(config: ConfigType, uart_ids: list[str]) -> None:
    validated = CONFIG_SCHEMA(config)
    assert [uart_id.id for uart_id in validated[CONF_UART_ID]] == uart_ids


def test_omitted_uart_id_is_one_unnamed_id() -> None:
    uart_id = CONFIG_SCHEMA({})[CONF_UART_ID]
    assert isinstance(uart_id, ID)
    assert uart_id.id is None


def test_omitted_uart_id_with_two_uarts_names_uart_id(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("two_uarts.yaml")
    assert read_config({}) is None
    assert "Too many candidates found for 'uart_id'" in capsys.readouterr().out


def test_rejects_an_empty_list() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_UART_ID: []})


@pytest.mark.parametrize(
    ("config_file", "parent", "listed"),
    [
        ("single.yaml", "uart_bus", []),
        (
            "usb_list.yaml",
            "zwave_ch34x_channel",
            ["zwave_ch34x_channel", "zwave_cdc_channel"],
        ),
    ],
)
def test_generated_uarts(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    parent: str,
    listed: list[str],
) -> None:
    main_cpp = generate_main(component_config_path(config_file))

    assert f"zw_proxy->set_uart_parent({parent});" in main_cpp
    assert re.findall(r"zw_proxy->add_uart\((\w+)\);", main_cpp) == listed
    counts = [str(d.value) for d in CORE.defines if d.name == "ZWAVE_PROXY_UART_COUNT"]
    assert counts == [str(len(listed))]
