"""Tests for the usb_audio buffer floors and the one-platform-per-hub check."""

import pytest

from esphome import config_validation as cv
from esphome.components.usb_audio import (
    CONF_USB_AUDIO_ID,
    microphone as usb_microphone,
    speaker as usb_speaker,
)
from esphome.config import Config
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_SIZE,
    CONF_NUM_CHANNELS,
    CONF_PLATFORM,
    CONF_SAMPLE_RATE,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _stream(sample_rate: int, channels: int, bits: int, buffer_size: int) -> ConfigType:
    return {
        CONF_SAMPLE_RATE: sample_rate,
        CONF_NUM_CHANNELS: channels,
        CONF_BITS_PER_SAMPLE: float(bits),
        CONF_BUFFER_SIZE: buffer_size,
    }


def _entry(hub: str, platform: str = "usb_audio") -> ConfigType:
    return {CONF_PLATFORM: platform, CONF_USB_AUDIO_ID: ID(hub)}


def _set_full_config(
    set_core_config: SetCoreConfigCallable, **domains: list[ConfigType]
) -> None:
    full = Config()
    full.update(domains)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)


def test_speaker_buffer_floor() -> None:
    # Two transfers of 3 packets x 48 frames x 4 bytes at 48 kHz 16-bit stereo.
    usb_speaker.validate_buffer_size(_stream(48000, 2, 16, 1152))
    with pytest.raises(cv.Invalid, match="at least 1152 bytes"):
        usb_speaker.validate_buffer_size(_stream(48000, 2, 16, 1151))


def test_speaker_floor_rounds_up_fractional_rates() -> None:
    # 44.1 kHz packets carry up to 45 frames.
    with pytest.raises(cv.Invalid, match="at least 1080 bytes"):
        usb_speaker.validate_buffer_size(_stream(44100, 2, 16, 1079))


def test_speaker_default_buffer_meets_floor() -> None:
    usb_speaker.validate_buffer_size(_stream(96000, 2, 32, 6400))


def test_microphone_buffer_floor() -> None:
    # 16 kHz 16-bit mono is 32000 bytes/s, so two read periods (40 ms) are 1280 bytes.
    usb_microphone.validate_buffer_size(_stream(16000, 1, 16, 1280))
    with pytest.raises(cv.Invalid, match="at least 1280 bytes"):
        usb_microphone.validate_buffer_size(_stream(16000, 1, 16, 1279))


def test_one_speaker_per_hub_accepted(set_core_config: SetCoreConfigCallable) -> None:
    entries = [_entry("hub_a"), _entry("hub_b"), _entry("hub_a", "i2s_audio")]
    _set_full_config(set_core_config, speaker=entries)
    usb_speaker.FINAL_VALIDATE_SCHEMA(entries[0])
    usb_speaker.FINAL_VALIDATE_SCHEMA(entries[1])


def test_two_speakers_on_one_hub_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    entries = [_entry("hub_a"), _entry("hub_a")]
    _set_full_config(set_core_config, speaker=entries)
    with pytest.raises(cv.Invalid, match="Only one usb_audio speaker"):
        usb_speaker.FINAL_VALIDATE_SCHEMA(entries[0])


def test_two_microphones_on_one_hub_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    entries = [_entry("hub_a"), _entry("hub_a")]
    _set_full_config(set_core_config, microphone=entries, speaker=[_entry("hub_a")])
    with pytest.raises(cv.Invalid, match="Only one usb_audio microphone"):
        usb_microphone.FINAL_VALIDATE_SCHEMA(entries[0])
