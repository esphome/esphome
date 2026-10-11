"""LVGL rejects ESP8266 at validation time."""

import pytest

from esphome.components import lvgl
from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_esp8266_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(Invalid, match="not supported on ESP8266"):
        lvgl.CONFIG_SCHEMA({})
