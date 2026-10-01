"""poll_period is capped so 2 * poll_period fits the uint16_t supervision check timeout."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C6
from esphome.components.openthread import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _config(poll_period: str) -> dict:
    return {
        "device_type": "MTD",
        "force_dataset": False,
        "tlv": "0e080000000000010000000300001035060004001fffe00208e227ac6a7f24052f0708fdb753eb517cb4d3051062b2442a928d9ea3b947a1618fc4085a030f4f70656e5468726561642d393837330102987304105330d857354330133c05e1fd7ae81a910c0402a0f7f8",
        "poll_period": poll_period,
    }


def test_poll_period_at_maximum_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C6}
    )
    assert CONFIG_SCHEMA(_config("32767s"))["poll_period"].total_milliseconds == (
        32767 * 1000
    )


def test_poll_period_above_maximum_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C6}
    )
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_config("32768s"))
