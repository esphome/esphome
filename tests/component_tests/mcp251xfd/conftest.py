"""Shared fixtures for the MCP251XFD tests."""

import pytest

from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.fixture(autouse=True)
def _set_esp32_core_config(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
