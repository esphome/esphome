"""Validation tests for the sound_level sensor's microphone channels.

A `test*.yaml` can only show that a configuration is accepted, so these cover the
rejections.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.sound_level.sensor import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.config import Config
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

MIC_ID = "mic"


def _full_config(mic_channels: int) -> Config:
    """A full config carrying one microphone, as the ID pass leaves it."""
    mic: ConfigType = {
        CONF_ID: ID(MIC_ID, is_declaration=True, type="microphone"),
        "min_channels": mic_channels,
        "max_channels": mic_channels,
    }
    full = Config()
    full["microphone"] = [mic]
    full.declare_ids.append((mic[CONF_ID], ["microphone", 0, CONF_ID]))
    return full


def _sensor_config(channels: list[int]) -> ConfigType:
    return CONFIG_SCHEMA(
        {
            "microphone": {
                "microphone": ID(MIC_ID, is_declaration=False),
                "channels": channels,
            },
            "passive": False,
        }
    )


def test_both_channels_of_a_stereo_microphone_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(2))

    FINAL_VALIDATE_SCHEMA(_sensor_config([0, 1]))


def test_channel_missing_from_the_microphone_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(1))

    with pytest.raises(cv.Invalid, match="Enabled channel 1 value must be at most 0"):
        FINAL_VALIDATE_SCHEMA(_sensor_config([0, 1]))
