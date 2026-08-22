"""Config validation for the custom_command -> custom_pdu rename.

The rename is load-bearing, not cosmetic: custom_command took a raw frame with a leading device
address byte, custom_pdu takes the PDU only. Re-using the old key would silently shift every frame
by one byte, so the old key must fail with migration guidance rather than validate.
"""

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.modbus_controller import ModbusItemBaseSchema
from esphome.components.modbus_controller.const import (
    CONF_CUSTOM_COMMAND,
    CONF_CUSTOM_PDU,
)


def test_custom_command_rejected_with_migration_text() -> None:
    """The removed key names its replacement and the semantic change."""
    with pytest.raises((Invalid, MultipleInvalid), match="renamed to 'custom_pdu'"):
        ModbusItemBaseSchema(
            {CONF_CUSTOM_COMMAND: [0x01, 0x03, 0x00, 0x2A, 0x00, 0x01]}
        )


def test_custom_pdu_accepted() -> None:
    """The new key takes PDU bytes (function code + data, no address byte)."""
    config = ModbusItemBaseSchema({CONF_CUSTOM_PDU: [0x03, 0x00, 0x2A, 0x00, 0x01]})
    assert config[CONF_CUSTOM_PDU] == [0x03, 0x00, 0x2A, 0x00, 0x01]


def test_custom_pdu_rejects_non_byte_values() -> None:
    """PDU entries are bytes; a word-sized value is a sign the old raw format is being used."""
    with pytest.raises((Invalid, MultipleInvalid)):
        ModbusItemBaseSchema({CONF_CUSTOM_PDU: [0x0103, 0x002A]})
