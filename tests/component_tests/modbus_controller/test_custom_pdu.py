"""Config validation for custom_pdu and the deprecated custom_command alias.

custom_command took a raw frame with a leading device address byte; custom_pdu takes the PDU only.
Most of these tests cover what the schema itself enforces (the two keys are mutually exclusive, and
custom_pdu takes byte-sized values). The last two reach the final-validate step that a bare-schema
test cannot: a write-coded custom_pdu polled continuously is rejected there.
"""

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.modbus_controller import (
    ModbusItemBaseSchema,
    validate_custom_pdu_item,
)
from esphome.components.modbus_controller.const import (
    CONF_CUSTOM_COMMAND,
    CONF_CUSTOM_PDU,
    CONF_MODBUS_CONTROLLER_ID,
)
from esphome.config import Config
from esphome.const import CONF_ADDRESS, CONF_CONTINUOUS, CONF_ID
from esphome.core import ID
import esphome.final_validate as fv


def test_custom_command_accepted_at_schema_level() -> None:
    """custom_command validates at the schema level; migration/rejection happens in final validate."""
    config = ModbusItemBaseSchema(
        {CONF_CUSTOM_COMMAND: [0x01, 0x03, 0x00, 0x2A, 0x00, 0x01]}
    )
    assert config[CONF_CUSTOM_COMMAND] == [0x01, 0x03, 0x00, 0x2A, 0x00, 0x01]


def test_custom_pdu_and_custom_command_mutually_exclusive() -> None:
    """Only one custom source may be given; supplying both is a schema error."""
    with pytest.raises((Invalid, MultipleInvalid)):
        ModbusItemBaseSchema(
            {
                CONF_CUSTOM_PDU: [0x03, 0x00, 0x2A, 0x00, 0x01],
                CONF_CUSTOM_COMMAND: [0x01, 0x03, 0x00, 0x2A, 0x00, 0x01],
            }
        )


def test_custom_pdu_accepted() -> None:
    """The new key takes PDU bytes (function code + data, no address byte)."""
    config = ModbusItemBaseSchema({CONF_CUSTOM_PDU: [0x03, 0x00, 0x2A, 0x00, 0x01]})
    assert config[CONF_CUSTOM_PDU] == [0x03, 0x00, 0x2A, 0x00, 0x01]


def test_custom_pdu_rejects_non_byte_values() -> None:
    """PDU entries are bytes; a word-sized value is a sign the old raw format is being used."""
    with pytest.raises((Invalid, MultipleInvalid)):
        ModbusItemBaseSchema({CONF_CUSTOM_PDU: [0x0103, 0x002A]})


def _controller_full_config(*, continuous: bool) -> Config:
    """A minimal full-config graph with one modbus_controller declaring id 'ctl', enough for the
    final-validate to resolve the controller (and its continuous flag) from an item's
    modbus_controller_id."""
    ctl_id = ID("ctl", is_declaration=True)
    config = Config()
    config["modbus_controller"] = [
        {CONF_ID: ctl_id, CONF_ADDRESS: 1, CONF_CONTINUOUS: continuous}
    ]
    config.declare_ids.append((ctl_id, ["modbus_controller", 0, CONF_ID]))
    return config


@pytest.fixture
def reset_full_config():
    token = fv.full_config.set(Config())
    yield
    fv.full_config.reset(token)


def test_continuous_write_custom_pdu_rejected(reset_full_config) -> None:
    """A write-coded custom_pdu (0x17 = read/write-multiple) under a continuous controller is
    rejected at final validate: the hub would strip continuous from the mutating code and warn on
    every update."""
    fv.full_config.set(_controller_full_config(continuous=True))
    with pytest.raises(Invalid, match="can't be polled continuously"):
        validate_custom_pdu_item(
            {
                CONF_MODBUS_CONTROLLER_ID: ID("ctl"),
                CONF_CUSTOM_PDU: [0x17, 0x00, 0x03, 0x00, 0x01],
            }
        )


def test_continuous_read_custom_pdu_allowed(reset_full_config) -> None:
    """A read-coded custom_pdu (0x03) under a continuous controller is fine - only writes stream."""
    fv.full_config.set(_controller_full_config(continuous=True))
    validate_custom_pdu_item(
        {
            CONF_MODBUS_CONTROLLER_ID: ID("ctl"),
            CONF_CUSTOM_PDU: [0x03, 0x00, 0x2A, 0x00, 0x01],
        }
    )
