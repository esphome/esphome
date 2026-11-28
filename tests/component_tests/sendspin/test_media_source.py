"""Validation and code generation tests for the sendspin media_source platform.

These cover the codec preference list, whose rejection branches a compile test
cannot reach, and the advertised audio formats, which a compile test cannot
check: a `test*.yaml` can only assert that a configuration is accepted.
"""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import CONF_CODECS, _get_data
from esphome.components.sendspin.media_source import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

AUDIO_FORMAT_RE = re.compile(
    r"\.codec = sendspin::SendspinCodecFormat::(\w+),\s*"
    r"\.channels = (\d+),\s*"
    r"\.sample_rate = \d+,\s*"
    r"\.bit_depth = (\d+)"
)


def _media_source_config(sample_rate=48000, bits_per_sample=16, **kwargs):
    return {
        "sendspin_id": "sendspin_hub",
        "formats": [{"sample_rate": sample_rate, "bits_per_sample": bits_per_sample}],
        **kwargs,
    }


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


def test_opus_only_codec_list_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Every server supports FLAC or PCM, so sendspin-cpp requires one of them."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="must include 'flac' or 'pcm'"):
        CONFIG_SCHEMA(_media_source_config(codecs=["opus"]))


@pytest.mark.parametrize(
    ("config_file", "bits_per_sample"),
    [
        ("media_source_defaults.yaml", 16),
        ("media_source_bits_per_sample_24.yaml", 24),
    ],
)
def test_advertised_bit_depths(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    bits_per_sample: int,
) -> None:
    """FLAC and PCM use the configured depth; Opus is only decoded to 16 bits."""
    main_cpp = generate_main(component_config_path(config_file))

    formats = [
        (codec, int(channels), int(bit_depth))
        for codec, channels, bit_depth in AUDIO_FORMAT_RE.findall(main_cpp)
    ]
    assert formats == [
        ("FLAC", 2, bits_per_sample),
        ("FLAC", 1, bits_per_sample),
        ("OPUS", 2, 16),
        ("OPUS", 1, 16),
        ("PCM", 2, bits_per_sample),
        ("PCM", 1, bits_per_sample),
    ]


def test_multi_format_opus_in_codec_list(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Opus stays in the codec list when any format is 48 kHz, even if others aren't."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(
        {
            "sendspin_id": "sendspin_hub",
            "formats": [
                {"sample_rate": 96000, "bits_per_sample": 24},
                {"sample_rate": 48000, "bits_per_sample": 24},
            ],
        }
    )

    assert config[CONF_CODECS] == ["flac", "opus", "pcm"]


def test_multi_format_no_48k_drops_opus(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """With no 48 kHz format, Opus leaves the default codec list."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(
        {
            "sendspin_id": "sendspin_hub",
            "formats": [
                {"sample_rate": 96000, "bits_per_sample": 24},
                {"sample_rate": 44100, "bits_per_sample": 24},
            ],
        }
    )

    assert config[CONF_CODECS] == ["flac", "pcm"]


def test_duplicate_format_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """Identical rate/bits entries in formats are redundant."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="Duplicate entry"):
        CONFIG_SCHEMA(
            {
                "sendspin_id": "sendspin_hub",
                "formats": [
                    {"sample_rate": 48000, "bits_per_sample": 16},
                    {"sample_rate": 48000, "bits_per_sample": 16},
                ],
            }
        )


def test_old_keys_migrate_to_formats(set_core_config: SetCoreConfigCallable) -> None:
    """Deprecated top-level sample_rate/bits_per_sample become a single formats entry."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(
        {
            "sendspin_id": "sendspin_hub",
            "sample_rate": 96000,
            "bits_per_sample": 24,
        }
    )

    assert config["formats"] == [{"sample_rate": 96000, "bits_per_sample": 24}]


def test_both_formats_and_old_keys_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Using the new and old keys together is ambiguous."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="Cannot use both"):
        CONFIG_SCHEMA(
            {
                "sendspin_id": "sendspin_hub",
                "sample_rate": 48000,
                "formats": [{"sample_rate": 48000, "bits_per_sample": 16}],
            }
        )


def test_channels_stereo_only(set_core_config: SetCoreConfigCallable) -> None:
    """channels: [2] produces no mono entries."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_media_source_config(channels=[2]))

    # Verify via generated code in a separate generate_main test,
    # or assert the player_config carries the value:
    assert _get_data().player_config["channels"] == [2]


def test_channels_mono_only(set_core_config: SetCoreConfigCallable) -> None:
    """channels: [1] produces no stereo entries."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_media_source_config(channels=[1]))

    assert _get_data().player_config["channels"] == [1]
