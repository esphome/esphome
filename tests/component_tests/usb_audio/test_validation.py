"""Tests for the usb_audio buffer floors, the one-platform-per-hub check and the USB FIFO split."""

import pytest

from esphome import config_validation as cv
from esphome.components import usb_audio
from esphome.components.esp32 import VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3
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
    KEY_IDF_VERSION,
    KEY_VARIANT,
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


def _unset_buffer(sample_rate: int, channels: int, bits: int) -> ConfigType:
    config = _stream(sample_rate, channels, bits, 0)
    del config[CONF_BUFFER_SIZE]
    return config


def test_default_buffer_size() -> None:
    assert (
        usb_speaker.validate_buffer_size(_unset_buffer(48000, 2, 16))[CONF_BUFFER_SIZE]
        == 6400
    )
    assert (
        usb_microphone.validate_buffer_size(_unset_buffer(16000, 1, 16))[
            CONF_BUFFER_SIZE
        ]
        == 6400
    )


def test_default_buffer_size_grows_to_the_floor() -> None:
    # 48 kHz 16-bit stereo is 192000 bytes/s, so 40 ms need 7680 bytes, more than the 6400 default.
    assert (
        usb_microphone.validate_buffer_size(_unset_buffer(48000, 2, 16))[
            CONF_BUFFER_SIZE
        ]
        == 7680
    )


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


IDF_5_5 = cv.Version(5, 5, 6)


def _fifo_config(
    set_core_config: SetCoreConfigCallable,
    variant: str,
    idf: cv.Version = IDF_5_5,
    **domains: list[ConfigType],
) -> Config:
    full = Config()
    full.update(domains)
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant, KEY_IDF_VERSION: idf},
        full_config=full,
    )
    return full


def _stream_entry(sample_rate: int, channels: int, bits: int) -> ConfigType:
    return {**_entry("hub_a"), **_stream(sample_rate, channels, bits, 6400)}


def test_fifo_split_fits_48k_stereo_speaker_and_microphone(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # 49 frames x 4 bytes = 196 bytes per packet each way; the 140 spare lines are shared.
    full = _fifo_config(
        set_core_config,
        VARIANT_ESP32S3,
        speaker=[_stream_entry(48000, 2, 16)],
        microphone=[_stream_entry(48000, 2, 16)],
    )
    rx, nptx, ptx = usb_audio.fifo_split(full)
    assert (rx, nptx, ptx) == (121, 16, 119)
    assert ptx * 4 >= 196
    assert (rx - 2) * 4 >= 196
    assert rx + nptx + ptx == 256


def test_fifo_split_speaker_only_gets_the_rest(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full = _fifo_config(
        set_core_config, VARIANT_ESP32S2, speaker=[_stream_entry(48000, 2, 16)]
    )
    assert usb_audio.fifo_split(full) == (34, 16, 206)


def test_fifo_split_only_on_full_speed_variants(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full = _fifo_config(
        set_core_config, VARIANT_ESP32P4, speaker=[_stream_entry(48000, 2, 16)]
    )
    assert usb_audio.fifo_split(full) is None


def test_fifo_split_rejects_formats_that_do_not_fit(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full = _fifo_config(
        set_core_config,
        VARIANT_ESP32S3,
        speaker=[_stream_entry(96000, 2, 32)],
        microphone=[_stream_entry(96000, 2, 32)],
    )
    with pytest.raises(cv.Invalid, match="bytes of USB FIFO"):
        usb_audio.fifo_split(full)


def test_fifo_split_needs_idf_5_5(set_core_config: SetCoreConfigCallable) -> None:
    _fifo_config(
        set_core_config,
        VARIANT_ESP32S3,
        idf=cv.Version(5, 4, 3),
        speaker=[_stream_entry(48000, 2, 16)],
    )
    with pytest.raises(cv.Invalid, match="needs ESP-IDF 5.5.0 or later"):
        usb_audio.FINAL_VALIDATE_SCHEMA({})
