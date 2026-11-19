"""Validation tests for the frameworks the sendspin hub builds on.

sendspin-cpp needs noise-c as an ESP-IDF component, which Arduino below IDF 6.0
cannot use, so the hub is ESP-IDF only.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

HUB_CONFIG = {"id": "sendspin_hub_id"}


def test_arduino_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_ARDUINO)

    with pytest.raises(cv.Invalid, match="esp-idf"):
        CONFIG_SCHEMA(HUB_CONFIG)


def test_esp_idf_accepted(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    assert CONFIG_SCHEMA(HUB_CONFIG)["id"].id == "sendspin_hub_id"
