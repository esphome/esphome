"""buffer_size is bytes on the pulse ring targets, with a floor and a 1000 pulse default."""

import pytest

from esphome.components import remote_receiver
from esphome.components.esp8266 import gpio as esp8266_gpio  # noqa: F401  registers the pin schema
from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validated(config: ConfigType) -> ConfigType:
    return remote_receiver.CONFIG_SCHEMA(config)


def test_pulse_ring_default_holds_1000_pulses(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    assert _validated({"pin": "GPIO4"})["buffer_size"] == 4000


def test_buffer_size_below_the_floor_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(Invalid, match="at least 64"):
        _validated({"pin": "GPIO4", "buffer_size": "32b"})
