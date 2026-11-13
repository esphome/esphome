"""Tests for the trf7962a config validation."""

from __future__ import annotations

import pytest

from esphome.components.spi import CONF_SPI_ID
from esphome.components.trf7962a import (
    DATA_RATE_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    _validate_no_shared_ethernet,
)
import esphome.config_validation as cv
from esphome.const import PlatformFramework
import esphome.final_validate as fv
from tests.component_tests.types import SetCoreConfigCallable


def test_spi_bus_shared_with_ethernet_is_rejected() -> None:
    fv.full_config.set({"ethernet": {CONF_SPI_ID: "bus"}})
    with pytest.raises(cv.Invalid, match="SPI Ethernet"):
        _validate_no_shared_ethernet({CONF_SPI_ID: "bus"})


@pytest.mark.parametrize("full_config", [{}, {"ethernet": {CONF_SPI_ID: "other"}}])
def test_spi_bus_not_shared_with_ethernet_is_accepted(full_config: dict) -> None:
    fv.full_config.set(full_config)
    _validate_no_shared_ethernet({CONF_SPI_ID: "bus"})


def test_final_validation_rejects_a_bus_shared_with_ethernet() -> None:
    fv.full_config.set({"ethernet": {CONF_SPI_ID: "bus"}})
    with pytest.raises(cv.Invalid, match="SPI Ethernet"):
        FINAL_VALIDATE_SCHEMA({CONF_SPI_ID: "bus"})


def test_data_rate_above_10mhz_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert DATA_RATE_SCHEMA("10MHz") == 10e6
    with pytest.raises(cv.Invalid, match="at most 10MHz"):
        DATA_RATE_SCHEMA("20MHz")
