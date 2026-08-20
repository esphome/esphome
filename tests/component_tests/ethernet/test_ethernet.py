"""Tests for the ethernet final-validation coexistence gate and schema bounds."""

import pytest
from voluptuous import Invalid

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    VARIANT_ESP32S3,
)
from esphome.components.ethernet import CONF_CLOCK_SPEED, CONFIG_SCHEMA, _final_validate
from esphome.components.network import _validate_priority_list
from esphome.const import CONF_PRIORITY, PlatformFramework
from esphome.core import CORE
import esphome.final_validate as fv

from ..types import SetCoreConfigCallable

_CH390_CONFIG = {
    "type": "CH390",
    "clk_pin": 47,
    "mosi_pin": 48,
    "miso_pin": 14,
    "cs_pin": 21,
}


@pytest.fixture(autouse=True)
def _reset_full_config():
    """Reset fv.full_config so each test starts with a clean slate."""
    token = fv.full_config.set({})
    yield
    fv.full_config.reset(token)


def test_rejects_wifi_and_ethernet_without_priority() -> None:
    """Wi-Fi + ethernet without a network: priority: list must be rejected."""
    fv.full_config.set({"wifi": {}, "ethernet": {}})
    with pytest.raises(Invalid, match="cannot be used together with component wifi"):
        _final_validate({})


def test_rejects_wifi_and_ethernet_with_incomplete_priority() -> None:
    """A priority list missing an interface is rejected and names what's missing."""
    fv.full_config.set(
        {
            "wifi": {},
            "ethernet": {},
            "network": {CONF_PRIORITY: _validate_priority_list(["ethernet"])},
        }
    )
    with pytest.raises(Invalid, match=r"must.*list both interfaces; missing: wifi"):
        _final_validate({})


@pytest.mark.parametrize("clock_speed", ["26.67MHz", "72MHz"])
def test_ch390_accepts_clock_speed_up_to_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable, clock_speed: str
) -> None:
    """CH390 SCK is rated to 72MHz, so the schema must accept the whole range."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    # _validate derives use_address from the node name, which has no default here.
    CORE.name = "ch390-test"
    config = CONFIG_SCHEMA({**_CH390_CONFIG, CONF_CLOCK_SPEED: clock_speed})
    assert config[CONF_CLOCK_SPEED] == cv.frequency(clock_speed)


def test_ch390_rejects_clock_speed_above_the_datasheet_maximum(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The shared 80MHz ceiling is out of spec for this part."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
            KEY_IDF_VERSION: cv.Version(5, 3, 2),
        },
    )
    # _validate derives use_address from the node name, which has no default here.
    CORE.name = "ch390-test"
    with pytest.raises(Invalid, match="value must be at most 72000000"):
        CONFIG_SCHEMA({**_CH390_CONFIG, CONF_CLOCK_SPEED: "80MHz"})
