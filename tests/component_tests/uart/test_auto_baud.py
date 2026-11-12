"""Tests for the ESP8266 software serial auto baud option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY
from esphome.components.uart import CONF_AUTO_BAUD_SYNC_BYTE, validate_auto_baud
from esphome.const import CONF_BAUD_RATE, CONF_RX_PIN, CONF_TX_PIN
from esphome.core import CORE
from esphome.types import ConfigType


def _config(sync: int, **overrides: object) -> ConfigType:
    config = {
        CONF_RX_PIN: {"number": 5},
        CONF_BAUD_RATE: 9600,
        CONF_DATA_BITS: 8,
        CONF_PARITY: "NONE",
        CONF_AUTO_BAUD_SYNC_BYTE: sync,
    }
    config.update(overrides)
    return config


def test_auto_baud_emits_setter_and_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("auto_baud_esp8266.yaml"))

    assert "gdo_uart->set_auto_baud_sync(0x55);" in main_cpp
    assert "USE_UART_AUTO_BAUD" in {d.name for d in CORE.defines}


@pytest.mark.parametrize(
    ("sync", "overrides"),
    [
        (0x55, {}),
        (0x55, {CONF_BAUD_RATE: 1200, CONF_PARITY: "EVEN"}),
        # Last edge at the start of bit 7
        (0xAA, {}),
        # The parity bit is 0, so the stop bit has an edge
        (0xFE, {CONF_PARITY: "ODD"}),
    ],
)
def test_valid_sync_bytes(sync: int, overrides: dict[str, object]) -> None:
    assert validate_auto_baud(_config(sync, **overrides))


@pytest.mark.parametrize(
    ("sync", "overrides", "match"),
    [
        (0x55, {CONF_RX_PIN: {"number": 3}}, "hardware UART"),
        (0x55, {CONF_RX_PIN: {"number": 13}, CONF_TX_PIN: {"number": 15}}, "hardware"),
        (0x55, {CONF_BAUD_RATE: 38400}, "up to 19200 baud"),
        (0x55, {CONF_DATA_BITS: 6}, "does not fit"),
        (0xFF, {}, "too few edges"),
        (0xF0, {}, "too few edges"),
        # Parity bit 1 continues the ones
        (0xFE, {CONF_PARITY: "EVEN"}, "too few edges"),
    ],
)
def test_invalid_sync_bytes(
    sync: int, overrides: dict[str, object], match: str
) -> None:
    with pytest.raises(cv.Invalid, match=match):
        validate_auto_baud(_config(sync, **overrides))
