"""Tests for the mqtt max_payload_size option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.mqtt import CONF_MAX_PAYLOAD_SIZE, CONFIG_SCHEMA
from esphome.const import CONF_BROKER, PlatformFramework
from esphome.core import CORE

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("config_file", "expected"),
    [("no_mac_suffix.yaml", 8192), ("max_payload_size.yaml", 4000)],
)
def test_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected: int,
) -> None:
    generate_main(component_config_path(config_file))
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["MQTT_MAX_PAYLOAD_SIZE"] == str(expected)


@pytest.mark.parametrize("value", [0, 511, 1048577])
def test_out_of_range_rejected(
    set_core_config: SetCoreConfigCallable, value: int
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match=CONF_MAX_PAYLOAD_SIZE):
        CONFIG_SCHEMA({CONF_BROKER: "192.168.1.2", CONF_MAX_PAYLOAD_SIZE: value})
