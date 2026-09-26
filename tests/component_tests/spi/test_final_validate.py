"""Tests for the _final_validate second-bus rejection in spi (Zephyr/nRF52)."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.spi import FINAL_VALIDATE_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

# _final_validate reads the buses from the full config, not from the config it is
# handed, so the argument below is just a placeholder.
_PLACEHOLDER_CONFIG: dict = {}


def _bus(bus_id: str) -> dict:
    return {"id": bus_id, "clk_pin": 14, "mosi_pin": 13, "miso_pin": 15}


def test_two_spi_buses_rejected_on_zephyr(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """A second SPI bus is not implemented on Zephyr and must be rejected."""
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    set_component_config("spi", [_bus("spi_bus"), _bus("spi_bus2")])

    with pytest.raises(cv.Invalid, match="Second spi is not implemented on Zephyr"):
        FINAL_VALIDATE_SCHEMA(_PLACEHOLDER_CONFIG)


def test_single_spi_bus_allowed_on_zephyr(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """A single SPI bus on Zephyr must pass final validation."""
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    set_component_config("spi", [_bus("spi_bus")])

    # Should not raise.
    FINAL_VALIDATE_SCHEMA(_PLACEHOLDER_CONFIG)


def test_two_spi_buses_allowed_off_zephyr(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """The rejection is Zephyr-only; other platforms allow multiple buses."""
    set_core_config(PlatformFramework.ESP32_IDF)
    set_component_config("spi", [_bus("spi_bus"), _bus("spi_bus2")])

    # Should not raise.
    FINAL_VALIDATE_SCHEMA(_PLACEHOLDER_CONFIG)
