"""Tests for the delta_wallbox component schema."""

from esphome.components.delta_wallbox import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.yaml_util import Sensitive
from tests.component_tests.types import SetCoreConfigCallable


def test_password_is_sensitive(set_core_config: SetCoreConfigCallable) -> None:
    """The BLE password is tagged for redaction and keeps its value."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA({"ble_client_id": "wallbox", "password": "secret"})
    assert isinstance(config["password"], Sensitive)
    assert config["password"] == "secret"
