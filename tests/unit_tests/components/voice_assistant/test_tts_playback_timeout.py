"""Validate Voice Assistant playback timeout configuration."""

import pytest

from esphome.components import voice_assistant
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE


@pytest.mark.parametrize(
    "value",
    ["0s", "0ms", {"seconds": 0}, "-1s", "500us", "1ns", "500ns", {"nanoseconds": 1}],
)
def test_playback_timeout_rejects_invalid_duration(value: str | dict[str, int]) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    with pytest.raises(cv.Invalid):
        voice_assistant.CONFIG_SCHEMA({"tts_playback_start_timeout": value})


@pytest.mark.parametrize(
    "value, milliseconds", [("1ms", 1), ("2s", 2000), ("10s", 10000)]
)
def test_playback_timeout_accepts_positive_milliseconds(
    value: str, milliseconds: int
) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = voice_assistant.CONFIG_SCHEMA({"tts_playback_start_timeout": value})
    assert config["tts_playback_start_timeout"].total_milliseconds == milliseconds


def test_playback_timeout_default_is_fifteen_seconds() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    config = voice_assistant.CONFIG_SCHEMA({})
    assert config["tts_playback_start_timeout"].total_milliseconds == 15000
