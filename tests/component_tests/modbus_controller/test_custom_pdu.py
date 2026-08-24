"""Schema-level config validation for custom_pdu and the deprecated custom_command alias.

custom_command took a raw frame with a leading device address byte; custom_pdu takes the PDU only.
The old key is still accepted at the schema level and auto-migrated later in final validate (which a
bare-schema test can't reach), so these tests only cover what the schema itself enforces: the two keys
are mutually exclusive, and custom_pdu takes byte-sized values.
"""

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.modbus_controller import ModbusItemBaseSchema
from esphome.components.modbus_controller.const import (
    CONF_CUSTOM_COMMAND,
    CONF_CUSTOM_PDU,
)


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
