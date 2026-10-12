"""Tests for ES7210 audio ADC configuration validation."""

import pytest

from esphome.components.es7210.audio_adc import CONF_CHANNEL_GAINS, CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN


def test_channel_gains_valid_config() -> None:
    """Per-channel gains accept an AEC microphone and reference configuration."""
    config = CONFIG_SCHEMA({CONF_CHANNEL_GAINS: ["33db", "33db", "0db", "0db"]})

    assert config[CONF_CHANNEL_GAINS] == [33.0, 33.0, 0.0, 0.0]
    assert CONF_MIC_GAIN not in config


def test_omitted_gains_remain_absent() -> None:
    """Code generation supplies the default when neither gain option is configured."""
    config = CONFIG_SCHEMA({})

    assert CONF_MIC_GAIN not in config
    assert CONF_CHANNEL_GAINS not in config


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
