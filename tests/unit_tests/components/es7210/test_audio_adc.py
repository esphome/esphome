"""Tests for ES7210 audio ADC configuration validation."""

import pytest

from esphome.components.es7210.audio_adc import CONF_CHANNEL_GAINS, CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN


@pytest.mark.parametrize("channel_count", [3, 5])
def test_channel_gains_requires_four_channels(channel_count: int) -> None:
    """Channel gains must provide one value for each ES7210 input."""
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_CHANNEL_GAINS: ["0db"] * channel_count})


def test_channel_gains_and_mic_gain_are_mutually_exclusive() -> None:
    """Uniform and per-channel gain configuration cannot be combined."""
    with pytest.raises(cv.Invalid, match="Cannot specify more than one"):
        CONFIG_SCHEMA(
            {
                CONF_MIC_GAIN: "24db",
                CONF_CHANNEL_GAINS: ["0db"] * 4,
            }
        )
