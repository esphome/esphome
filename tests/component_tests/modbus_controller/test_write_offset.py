"""Config validation for the byte offset on holding-register write entities.

A 16-bit register write cannot target half a register, so an odd offset (or byte_offset) is
rejected for holding-register switches and outputs; even offsets and coil offsets pass.
"""

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.modbus_controller.output import (
    CONFIG_SCHEMA as OUTPUT_CONFIG_SCHEMA,
)
from esphome.components.modbus_controller.switch import (
    CONFIG_SCHEMA as SWITCH_CONFIG_SCHEMA,
)
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_OFFSET


def _switch_config(register_type: str, offset: int) -> dict:
    return {
        CONF_NAME: "test switch",
        CONF_ADDRESS: 0x10,
        "register_type": register_type,
        CONF_OFFSET: offset,
    }


def _output_config(register_type: str, offset: int) -> dict:
    return {
        CONF_ID: "test_output",
        CONF_ADDRESS: 0x10,
        "register_type": register_type,
        CONF_OFFSET: offset,
    }


def test_odd_offset_on_holding_switch_rejected() -> None:
    with pytest.raises((Invalid, MultipleInvalid), match="odd"):
        SWITCH_CONFIG_SCHEMA(_switch_config("holding", 3))


def test_even_offset_on_holding_switch_accepted() -> None:
    config = SWITCH_CONFIG_SCHEMA(_switch_config("holding", 2))
    assert config[CONF_OFFSET] == 2


def test_odd_offset_on_coil_switch_accepted() -> None:
    """A coil offset is a coil count, so odd values are fine."""
    config = SWITCH_CONFIG_SCHEMA(_switch_config("coil", 3))
    assert config[CONF_OFFSET] == 3


def test_odd_byte_offset_on_holding_switch_rejected() -> None:
    """byte_offset is the alias the validator must also catch."""
    config = _switch_config("holding", 0)
    del config[CONF_OFFSET]
    config["byte_offset"] = 3
    with pytest.raises((Invalid, MultipleInvalid), match="byte_offset"):
        SWITCH_CONFIG_SCHEMA(config)


def test_odd_offset_on_holding_output_rejected() -> None:
    with pytest.raises((Invalid, MultipleInvalid), match="odd"):
        OUTPUT_CONFIG_SCHEMA(_output_config("holding", 3))


def test_even_offset_on_holding_output_accepted() -> None:
    config = OUTPUT_CONFIG_SCHEMA(_output_config("holding", 2))
    assert config[CONF_OFFSET] == 2


def test_odd_offset_on_coil_output_accepted() -> None:
    config = OUTPUT_CONFIG_SCHEMA(_output_config("coil", 3))
    assert config[CONF_OFFSET] == 3
