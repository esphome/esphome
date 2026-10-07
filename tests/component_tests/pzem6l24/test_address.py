"""The PZEM-6L24 answers unit addresses 1 to 247 only."""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus
from esphome.components.pzem6l24.sensor import CONFIG_SCHEMA
from esphome.const import CONF_ADDRESS
from esphome.types import ConfigType


def _sensor(**extra: object) -> ConfigType:
    return CONFIG_SCHEMA({modbus.CONF_MODBUS_ID: "bus", **extra})


def test_address_defaults_to_one() -> None:
    assert _sensor()[CONF_ADDRESS] == 1


@pytest.mark.parametrize("address", [1, 247])
def test_address_in_unit_range_accepted(address: int) -> None:
    assert _sensor(**{CONF_ADDRESS: address})[CONF_ADDRESS] == address


@pytest.mark.parametrize("address", [0, 248, 255])
def test_address_outside_unit_range_rejected(address: int) -> None:
    with pytest.raises(cv.Invalid):
        _sensor(**{CONF_ADDRESS: address})
