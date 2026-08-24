"""Tests for the http_request watchdog timeout default."""

import pytest

from esphome.components.http_request import (
    WATCHDOG_TIMEOUT_MULTIPLIER,
    default_watchdog_timeout,
)
from esphome.const import CONF_TIMEOUT, CONF_WATCHDOG_TIMEOUT, PlatformFramework
from esphome.core import TimePeriodMilliseconds

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "platform", [PlatformFramework.ESP32_IDF, PlatformFramework.ESP32_ARDUINO]
)
def test_esp32_derives_watchdog_timeout(
    set_core_config: SetCoreConfigCallable, platform: PlatformFramework
) -> None:
    """ESP32 arms the watchdog at a multiple of the request timeout."""
    set_core_config(platform)
    config = default_watchdog_timeout(
        {CONF_TIMEOUT: TimePeriodMilliseconds(milliseconds=4500)}
    )
    assert config[CONF_WATCHDOG_TIMEOUT] == TimePeriodMilliseconds(
        milliseconds=4500 * WATCHDOG_TIMEOUT_MULTIPLIER
    )


def test_esp32_keeps_explicit_watchdog_timeout(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A user supplied watchdog timeout is not overridden."""
    set_core_config(PlatformFramework.ESP32_IDF)
    explicit = TimePeriodMilliseconds(milliseconds=20000)
    config = default_watchdog_timeout(
        {
            CONF_TIMEOUT: TimePeriodMilliseconds(milliseconds=10000),
            CONF_WATCHDOG_TIMEOUT: explicit,
        }
    )
    assert config[CONF_WATCHDOG_TIMEOUT] == explicit


@pytest.mark.parametrize(
    "platform",
    [
        PlatformFramework.ESP8266_ARDUINO,
        PlatformFramework.RP2040_ARDUINO,
        PlatformFramework.HOST_NATIVE,
    ],
)
def test_other_platforms_leave_watchdog_unset(
    set_core_config: SetCoreConfigCallable, platform: PlatformFramework
) -> None:
    """No default is injected where the request watchdog is unsupported or capped."""
    set_core_config(platform)
    config = default_watchdog_timeout(
        {CONF_TIMEOUT: TimePeriodMilliseconds(milliseconds=4500)}
    )
    assert CONF_WATCHDOG_TIMEOUT not in config
