"""Tests for the nextion TFT upload watchdog timeout validation."""

import pytest

from esphome.components.nextion.base_component import CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT
from esphome.components.nextion.display import FINAL_VALIDATE_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_WATCHDOG_TIMEOUT, PLATFORM_ESP32, PlatformFramework
from esphome.core import TimePeriod, TimePeriodMilliseconds
from tests.component_tests.types import SetCoreConfigCallable

NEXTION_CONFIG = {CONF_TFT_UPLOAD_WATCHDOG_TIMEOUT: TimePeriodMilliseconds(seconds=30)}


def _esp32(set_core_config: SetCoreConfigCallable, watchdog_seconds: int) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={
            PLATFORM_ESP32: {
                CONF_WATCHDOG_TIMEOUT: TimePeriod(seconds=watchdog_seconds)
            }
        },
    )


def test_tft_upload_watchdog_accepted_when_platform_enabled(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _esp32(set_core_config, 5)
    FINAL_VALIDATE_SCHEMA(NEXTION_CONFIG)


def test_tft_upload_watchdog_rejected_when_platform_disabled(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _esp32(set_core_config, 0)
    with pytest.raises(cv.Invalid, match="Cannot be set when esp32.watchdog_timeout"):
        FINAL_VALIDATE_SCHEMA(NEXTION_CONFIG)
