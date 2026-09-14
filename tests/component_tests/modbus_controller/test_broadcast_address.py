"""A modbus_controller cannot poll the broadcast address (0) unless allow_broadcast_read says the
device answers it."""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus
from esphome.components.modbus_controller import CONFIG_SCHEMA
from esphome.const import CONF_ADDRESS
from esphome.types import ConfigType


def _controller(address: int, **extra: object) -> ConfigType:
    return CONFIG_SCHEMA({modbus.CONF_MODBUS_ID: "bus", CONF_ADDRESS: address, **extra})


def test_address_zero_rejected_by_default() -> None:
    with pytest.raises(cv.Invalid, match="broadcast address"):
        _controller(0)


def test_address_zero_accepted_with_allow_broadcast_read() -> None:
    config = _controller(0, **{modbus.CONF_ALLOW_BROADCAST_READ: True})
    assert config[CONF_ADDRESS] == 0
    assert config[modbus.CONF_ALLOW_BROADCAST_READ] is True


def test_allow_broadcast_read_defaults_false() -> None:
    assert _controller(1)[modbus.CONF_ALLOW_BROADCAST_READ] is False


def test_writer_entity_takes_expect_broadcast_write_response() -> None:
    # The write-side option lives on the writing platforms, not the controller.
    from esphome.components.modbus_controller.const import CONF_MODBUS_CONTROLLER_ID
    from esphome.components.modbus_controller.switch import (
        CONFIG_SCHEMA as SWITCH_SCHEMA,
    )
    from esphome.const import CONF_NAME

    key = modbus.CONF_EXPECT_BROADCAST_WRITE_RESPONSE
    base = {
        CONF_MODBUS_CONTROLLER_ID: "ctl",
        CONF_NAME: "Switch",
        "register_type": "coil",
        CONF_ADDRESS: 0x20,
    }
    assert SWITCH_SCHEMA(base)[key] is False
    assert SWITCH_SCHEMA({**base, CONF_NAME: "Switch 2", key: True})[key] is True
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({modbus.CONF_MODBUS_ID: "bus", CONF_ADDRESS: 1, key: True})
