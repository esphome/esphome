"""Test hexadecimal Thread credentials through the connection schema."""

from esphome import yaml_util
from esphome.components import openthread
from esphome.core import HexInt


def test_network_key_is_masked_without_changing_sdkconfig_value() -> None:
    """Keep the 128-bit numeric key used to generate ESP-IDF settings."""
    key = "0xdfd34f0f05cad978ec4e32b0413038ff"
    component = openthread._CONNECTION_SCHEMA({"network_key": key, "pan_id": 0x8F28})
    validated = component["network_key"]
    assert isinstance(validated, HexInt)
    assert f"{validated:X}".lower() == key[2:]
    out = yaml_util.dump(component)
    assert f"\\033[8m{validated}\\033[28m" in out
    assert "pan_id: 0x8F28" in out
