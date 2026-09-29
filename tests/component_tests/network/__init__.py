"""Shared helpers for the network component tests."""

from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.core import CORE


def sdkconfig_option(name: str) -> int | bool | None:
    """Return a generated sdkconfig value, or None when unset or not ESP32."""
    if KEY_ESP32 not in CORE.data:
        return None
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS].get(name)
