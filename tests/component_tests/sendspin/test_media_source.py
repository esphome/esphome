"""Validation tests for the sendspin media_source platform.

These cover the codec preference list, whose rejection branches a compile test
cannot reach: a `test*.yaml` can only assert that a configuration is accepted.
"""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import CONF_CODECS, _get_data
from esphome.components.sendspin.media_source import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _media_source_config(**overrides: Any) -> ConfigType:
    """Build a minimal valid media source config, allowing field overrides."""
    config: ConfigType = {
        "id": "sendspin_media_source",
        "sendspin_id": "sendspin_hub",
    }
    config.update(overrides)
    return config


def test_default_codecs_at_48_khz(set_core_config: SetCoreConfigCallable) -> None:
    """Every codec is advertised when the sample rate suits all of them."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(_media_source_config())

    assert config[CONF_CODECS] == ["flac", "opus", "pcm"]


def test_default_codecs_drop_opus_at_other_rates(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Opus only supports 48 kHz, so it leaves the default list at other rates."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(_media_source_config(sample_rate=44100))

    assert config[CONF_CODECS] == ["flac", "pcm"]


def test_configured_order_is_preserved(set_core_config: SetCoreConfigCallable) -> None:
    """The list is a preference order, so it reaches the player role as written."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_media_source_config(codecs=["pcm", "flac"]))

    assert _get_data().player_config[CONF_CODECS] == ["pcm", "flac"]


def test_empty_codec_list_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """A player with no codecs at all could never be given a stream."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="length of value must be at least 1"):
        CONFIG_SCHEMA(_media_source_config(codecs=[]))


def test_duplicate_codec_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """A repeated codec has no meaning in a preference order."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="may only be listed once"):
        CONFIG_SCHEMA(_media_source_config(codecs=["flac", "flac"]))


def test_unknown_codec_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """Only codecs the player role can decode are accepted."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="Unknown value"):
        CONFIG_SCHEMA(_media_source_config(codecs=["mp3"]))


def test_opus_at_wrong_sample_rate_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Asking for Opus at a rate it cannot handle fails rather than silently
    dropping the stated preference."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="requires a sample_rate of 48000"):
        CONFIG_SCHEMA(_media_source_config(codecs=["opus"], sample_rate=44100))
