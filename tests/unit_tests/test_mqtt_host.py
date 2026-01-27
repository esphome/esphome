from __future__ import annotations

from copy import deepcopy

import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
)
from esphome.core import CORE


def test_mqtt_schema_allows_host_platform() -> None:
    from esphome.components import mqtt

    old_data = deepcopy(CORE.data)
    old_name = CORE.name
    try:
        CORE.data.clear()
        CORE.data[KEY_CORE] = {
            KEY_TARGET_PLATFORM: PLATFORM_HOST,
            KEY_TARGET_FRAMEWORK: "host",
            KEY_FRAMEWORK_VERSION: cv.Version(1, 0, 0),
        }
        CORE.name = "testnode"

        validated = mqtt.CONFIG_SCHEMA({"broker": "127.0.0.1"})
        assert validated["broker"] == "127.0.0.1"
    finally:
        CORE.data.clear()
        CORE.data.update(old_data)
        CORE.name = old_name
