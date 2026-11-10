"""Unit tests for the distance_uart sensor component."""

from __future__ import annotations

import logging
from typing import Any

import pytest

from esphome import config as esphome_config
import esphome.codegen as cg
from esphome.components.a01nyub import sensor as a01nyub_sensor
from esphome.components.a02yyuw import sensor as a02yyuw_sensor
from esphome.components.distance_uart import sensor as distance_uart
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM
from esphome.core import CORE
from esphome.types import ConfigType


def _setup_core() -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "esp32",
        KEY_TARGET_FRAMEWORK: "esp-idf",
    }
    CORE.data["esp32"] = {"variant": "ESP32", "board": "esp32dev"}
    esphome_config.path_context.set([])


def test_accepts_lowercase_model() -> None:
    _setup_core()
    config = distance_uart.CONFIG_SCHEMA(
        {
            "model": "a02yyuw",
            "name": "Water Level",
        }
    )
    assert config["model"] == "A02YYUW"
    assert distance_uart._get_effective_mode(config) == "AUTO"


def test_unknown_model_raises_cv_invalid() -> None:
    _setup_core()
    with pytest.raises(cv.Invalid, match="Unknown value 'A02YYUV'"):
        distance_uart.CONFIG_SCHEMA(
            {
                "model": "A02YYUV",
            }
        )


@pytest.mark.parametrize(
    ("raw_config", "match"),
    [
        (
            {"model": "A02YYUW", "mode": "AUTO"},
            "cannot manually specify 'mode' when using model 'A02YYUW'",
        ),
        (
            {"model": "A02YYUW", "blind_zone": "5cm"},
            "cannot manually specify 'blind_zone' or 'max_range'",
        ),
        (
            {"model": "A07NYUB", "output_mode_pin": 4},
            "Model 'A07NYUB' does not support 'output_mode' or 'output_mode_pin'",
        ),
        (
            {"mode": "AUTO", "trigger_pin": 4},
            "`trigger_pin` is not allowed when `mode` is 'AUTO'",
        ),
        (
            {"model": "A02YYUW", "output_mode": "REALTIME"},
            "'output_mode' requires 'output_mode_pin' to be configured",
        ),
        (
            {"mode": "CONTROLLED", "output_mode_pin": 4},
            "'output_mode' and 'output_mode_pin' are only allowed when 'mode' is 'AUTO'",
        ),
    ],
)
def test_invalid_user_configs(raw_config: dict[str, Any], match: str) -> None:
    _setup_core()
    with pytest.raises(cv.Invalid, match=match):
        distance_uart.CONFIG_SCHEMA({"name": "Test", **raw_config})


@pytest.mark.asyncio
async def test_codegen_auto_mode_without_output_mode_pin() -> None:
    _setup_core()
    config: ConfigType = distance_uart.CONFIG_SCHEMA(
        {
            "id": "dist_sensor",
            "uart_id": "uart_bus",
            "model": "A02YYUW",
        }
    )
    cg.new_Pvariable(config["uart_id"])
    CORE.component_ids.add("dist_sensor")

    await distance_uart.to_code(config)
    CORE.flush_tasks()

    source = CORE.cpp_main_section
    assert (
        "set_mode(distance_uart::DistanceUARTMode::DISTANCE_UART_MODE_AUTO)" in source
    )
    assert "set_blind_zone(0.03f)" in source
    assert "set_max_range(4.5f)" in source
    assert "set_trigger_pin" not in source
    assert "set_output_mode_pin" not in source
    assert "set_output_mode(" not in source


@pytest.mark.asyncio
async def test_codegen_auto_mode_with_output_mode_pin() -> None:
    _setup_core()
    config: ConfigType = distance_uart.CONFIG_SCHEMA(
        {
            "id": "dist_sensor",
            "uart_id": "uart_bus",
            "model": "A02YYUW",
            "output_mode_pin": 5,
            "output_mode": "REALTIME",
        }
    )
    cg.new_Pvariable(config["uart_id"])
    CORE.component_ids.add("dist_sensor")

    await distance_uart.to_code(config)
    CORE.flush_tasks()

    source = CORE.cpp_main_section
    assert (
        "set_mode(distance_uart::DistanceUARTMode::DISTANCE_UART_MODE_AUTO)" in source
    )
    assert "set_output_mode_pin(" in source
    assert (
        "set_output_mode(distance_uart::DistanceUARTOutputMode::DISTANCE_UART_OUTPUT_MODE_REALTIME)"
        in source
    )


@pytest.mark.asyncio
async def test_codegen_controlled_mode_with_trigger_pin() -> None:
    _setup_core()
    config: ConfigType = distance_uart.CONFIG_SCHEMA(
        {
            "id": "dist_sensor",
            "uart_id": "uart_bus",
            "model": "A02YYT",
            "trigger_pin": 4,
        }
    )
    cg.new_Pvariable(config["uart_id"])
    CORE.component_ids.add("dist_sensor")

    await distance_uart.to_code(config)
    CORE.flush_tasks()

    source = CORE.cpp_main_section
    assert (
        "set_mode(distance_uart::DistanceUARTMode::DISTANCE_UART_MODE_CONTROLLED)"
        in source
    )
    assert "set_trigger_pin(" in source


def test_deprecated_components_warn(caplog: pytest.LogCaptureFixture) -> None:
    with caplog.at_level(logging.WARNING, logger=a01nyub_sensor.__name__):
        a01nyub_sensor._warn_deprecated({})
    assert "a01nyub" in caplog.text
    assert "distance_uart" in caplog.text

    caplog.clear()
    with caplog.at_level(logging.WARNING, logger=a02yyuw_sensor.__name__):
        a02yyuw_sensor._warn_deprecated({})
    assert "a02yyuw" in caplog.text
    assert "meters instead of millimeters" in caplog.text
