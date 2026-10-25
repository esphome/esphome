"""Configuration tests for the MCP2515 CAN controller."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.mcp2515 import canbus as mcp2515
import esphome.config_validation as cv
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def _set_esp32_core_config(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )


def _validate(options: dict[str, Any]) -> dict[str, Any]:
    return mcp2515.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 5, **options})


def test_defaults() -> None:
    config = _validate({})
    assert config["clock"] == "8MHZ"
    assert config["mode"] == "NORMAL"
    assert config["bit_rate"] == "125KBPS"
    assert config["default_fd"] is False
    assert config["default_bit_rate_switch"] is False
    assert config["default_pad_data"] is False


@pytest.mark.parametrize("clock", ["8MHZ", "12MHZ", "16MHZ", "20MHZ"])
def test_clock_values(clock: str) -> None:
    assert _validate({"clock": clock})["clock"] == clock


@pytest.mark.parametrize("mode", ["NORMAL", "LOOPBACK", "LISTENONLY"])
def test_mode_values(mode: str) -> None:
    assert _validate({"mode": mode})["mode"] == mode


@pytest.mark.parametrize(
    "key,value", [("clock", "40MHZ"), ("mode", "CLASSIC"), ("bit_rate", "2000KBPS")]
)
def test_unsupported_options_are_rejected(key: str, value: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate({key: value})


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main = generate_main(component_config_path("mcp2515.yaml"))
    assert "set_mcp_clock(" in main and "MCP_12MHZ" in main
    assert "set_mcp_mode(" in main and "CANCTRL_REQOP_LISTENONLY" in main
    assert "set_bitrate(" in main and "CAN_500KBPS" in main
    assert "set_can_id(291)" in main
