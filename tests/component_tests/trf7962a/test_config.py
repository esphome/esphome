"""Tests for the trf7962a config validation."""

from __future__ import annotations

import pytest

from esphome.components.spi import CONF_SPI_ID
from esphome.components.trf7962a import _validate_no_shared_ethernet
import esphome.config_validation as cv
import esphome.final_validate as fv


def test_spi_bus_shared_with_ethernet_is_rejected() -> None:
    fv.full_config.set({"ethernet": {CONF_SPI_ID: "bus"}})
    with pytest.raises(cv.Invalid, match="SPI Ethernet"):
        _validate_no_shared_ethernet({CONF_SPI_ID: "bus"})


@pytest.mark.parametrize("full_config", [{}, {"ethernet": {CONF_SPI_ID: "other"}}])
def test_spi_bus_not_shared_with_ethernet_is_accepted(full_config: dict) -> None:
    fv.full_config.set(full_config)
    _validate_no_shared_ethernet({CONF_SPI_ID: "bus"})
