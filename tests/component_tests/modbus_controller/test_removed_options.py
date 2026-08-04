"""The removed modbus_controller options must fail validation pointing at their replacements."""

import pytest
from voluptuous import Invalid

from esphome.components.modbus_controller import CONFIG_SCHEMA


@pytest.mark.parametrize(
    ("option", "value", "match"),
    [
        ("allow_duplicate_commands", False, r"collapsed"),
        ("command_throttle", "200ms", r"turnaround_time"),
        ("server_courtesy_response", True, r"modbus_server"),
        ("server_registers", [], r"modbus_server"),
    ],
)
def test_removed_option_is_rejected(option: str, value: object, match: str) -> None:
    """Each removed option raises with a message naming its replacement."""
    with pytest.raises(Invalid, match=match):
        CONFIG_SCHEMA({option: value})
