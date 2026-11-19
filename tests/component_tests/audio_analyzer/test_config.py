"""Validation tests for audio_analyzer.

A `test*.yaml` can only show that a configuration is accepted, so these cover the rejections and how the speaker
platform's declared format is checked against its output and by the components that write to it.
"""

import pytest

from esphome import config_validation as cv
from esphome.components import audio
from esphome.components.audio_analyzer import (
    CONFIG_SCHEMA as MIC_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as MIC_FINAL_VALIDATE_SCHEMA,
    analysis_rate,
)
from esphome.components.audio_analyzer.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.components.audio_analyzer.speaker import (
    CONFIG_SCHEMA as SPEAKER_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as SPEAKER_FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

MIC_ID = "mic"
OUTPUT_ID = "output"
ANALYZER_ID = "analyzer"


def _full_config(output: ConfigType, analyzer: ConfigType | None = None) -> Config:
    """A full config with one output speaker, one microphone and optionally the analyzer, as the ID pass leaves it."""
    full = Config()
    mic_sample_rate = output.pop("mic_sample_rate", 16000)
    output = {CONF_ID: ID(OUTPUT_ID, is_declaration=True, type="speaker"), **output}
    mic = {
        CONF_ID: ID(MIC_ID, is_declaration=True, type="microphone"),
        "sample_rate": mic_sample_rate,
        "min_channels": 1,
        "max_channels": 1,
    }
    full["speaker"] = [output] + ([analyzer] if analyzer is not None else [])
    full["microphone"] = [mic]
    full.declare_ids.append((output[CONF_ID], ["speaker", 0, CONF_ID]))
    full.declare_ids.append((mic[CONF_ID], ["microphone", 0, CONF_ID]))
    if analyzer is not None:
        full.declare_ids.append((analyzer[CONF_ID], ["speaker", 1, CONF_ID]))
    return full


SPEAKER_FORMAT = {"bits_per_sample": 16, "num_channels": 1, "sample_rate": 48000}


def _speaker_config(**options) -> ConfigType:
    return SPEAKER_CONFIG_SCHEMA(
        {
            CONF_ID: ID(ANALYZER_ID, is_declaration=True, type="speaker"),
            "output_speaker": ID(OUTPUT_ID, is_declaration=False),
            **SPEAKER_FORMAT,
            **options,
        }
    )


def _mic_config(**options) -> ConfigType:
    return MIC_CONFIG_SCHEMA(
        {
            "microphone": {"microphone": ID(MIC_ID, is_declaration=False)},
            "passive": True,
            **options,
        }
    )


@pytest.fixture(autouse=True)
def esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config({}))


@pytest.mark.parametrize(
    ("options", "message"),
    [
        (
            {"rms_range": {"min_db": -20, "max_db": -20}},
            "min_db must be below max_db",
        ),
        ({"band_range": {"min_db": -10, "max_db": -30}}, "min_db must be below max_db"),
        (
            {"min_frequency": "3000Hz", "max_frequency": "2000Hz"},
            "min_frequency must be below max_frequency",
        ),
        (
            {"min_frequency": "20Hz", "max_frequency": "200Hz"},
            "raise max_frequency to at least 766 Hz",
        ),
        (
            {"min_frequency": "4000Hz", "max_frequency": "4700Hz"},
            "raise max_frequency to at least 4735 Hz",
        ),
        ({"max_frequency": "4900Hz"}, "value must be at most 4800"),
    ],
)
def test_analysis_options_rejected(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _mic_config(**options)


@pytest.mark.parametrize(
    ("min_frequency", "max_frequency"),
    [("20Hz", "800Hz"), ("4000Hz", "4800Hz"), ("100Hz", "4800Hz")],
)
def test_narrowest_ranges_accepted(min_frequency: str, max_frequency: str) -> None:
    _mic_config(min_frequency=min_frequency, max_frequency=max_frequency)


def test_default_band_plan_accepted() -> None:
    config = _mic_config()
    assert config["min_frequency"] == 100
    assert config["max_frequency"] == 4000


@pytest.mark.parametrize(
    ("sample_rate", "expected"),
    [(8000, 8000), (16000, 16000), (22050, 11025), (44100, 14700), (48000, 16000)],
)
def test_analysis_rate(sample_rate: int, expected: float) -> None:
    assert analysis_rate(sample_rate) == expected


def test_max_frequency_above_microphone_rate_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config({"mic_sample_rate": 8000})
    )
    with pytest.raises(cv.Invalid, match="at most 2400 Hz for a 8000 Hz stream"):
        MIC_FINAL_VALIDATE_SCHEMA(_mic_config())


def test_microphone_rate_unknown_at_config_time_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The rate is then only known at run time, where the bands are clamped with a warning
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config({"mic_sample_rate": None}),
    )
    MIC_FINAL_VALIDATE_SCHEMA(_mic_config(max_frequency="4800Hz"))


@pytest.mark.parametrize(
    ("options", "message"),
    [
        ({"buffer_duration": "0ms"}, "buffer_duration"),
        ({"buffer_duration": "50ms"}, "buffer_duration"),
        ({"audio_dac": "dac"}, "set audio_dac on the output speaker"),
        ({"sample_rate": 96000}, "sample_rate"),
    ],
)
def test_speaker_options_rejected(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _speaker_config(**options)


@pytest.mark.parametrize("key", ["bits_per_sample", "num_channels", "sample_rate"])
def test_speaker_format_required(key: str) -> None:
    config = {
        CONF_ID: ID(ANALYZER_ID, is_declaration=True, type="speaker"),
        "output_speaker": ID(OUTPUT_ID, is_declaration=False),
        **SPEAKER_FORMAT,
    }
    del config[key]
    with pytest.raises(
        cv.Invalid, match=f"required key not provided @ data\\['{key}'\\]"
    ):
        SPEAKER_CONFIG_SCHEMA(config)


def test_speaker_limits_set_by_its_schema() -> None:
    # Set at CONFIG_SCHEMA time, so components validated before this speaker's FINAL_VALIDATE see them
    analyzer = _speaker_config()
    for key, value in (
        ("bits_per_sample", 16),
        ("channels", 1),
        ("sample_rate", 48000),
    ):
        assert analyzer[f"min_{key}"] == value
        assert analyzer[f"max_{key}"] == value


@pytest.mark.parametrize(
    ("sample_rate", "channels", "valid"),
    [(48000, 1, True), (16000, 1, False), (48000, 2, False)],
)
def test_producer_validated_against_declared_format(
    set_core_config: SetCoreConfigCallable,
    sample_rate: int,
    channels: int,
    valid: bool,
) -> None:
    # What a media player pipeline runs on its speaker; works whether or not the analyzer's own final validation ran
    analyzer = _speaker_config()
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config({}, analyzer))
    validate = audio.final_validate_audio_schema(
        "speaker media_player",
        audio_device="speaker",
        bits_per_sample=16,
        channels=channels,
        sample_rate=sample_rate,
    )
    producer = {"speaker": ID(ANALYZER_ID, is_declaration=False)}
    if valid:
        validate(producer)
    else:
        with pytest.raises(
            cv.Invalid, match="Invalid configuration for the speaker media_player"
        ):
            validate(producer)


@pytest.mark.parametrize(
    ("output", "message"),
    [
        ({"min_sample_rate": 16000, "max_sample_rate": 16000}, "sample_rate"),
        ({"min_bits_per_sample": 32, "max_bits_per_sample": 32}, "bits_per_sample"),
    ],
)
def test_speaker_format_checked_against_output(
    set_core_config: SetCoreConfigCallable, output: ConfigType, message: str
) -> None:
    analyzer = _speaker_config()
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(output, analyzer)
    )
    with pytest.raises(
        cv.Invalid, match=f"audio_analyzer speaker component. The {message}"
    ):
        SPEAKER_FINAL_VALIDATE_SCHEMA(analyzer)


def test_speaker_format_accepted_by_output(
    set_core_config: SetCoreConfigCallable,
) -> None:
    analyzer = _speaker_config()
    output = {"min_bits_per_sample": 16, "max_bits_per_sample": 32}
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(output, analyzer)
    )
    SPEAKER_FINAL_VALIDATE_SCHEMA(analyzer)


def test_speaker_max_frequency_above_its_rate_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    analyzer = _speaker_config(sample_rate=8000, max_frequency="4000Hz")
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config({}, analyzer))
    with pytest.raises(cv.Invalid, match="at most 2400 Hz for a 8000 Hz stream"):
        SPEAKER_FINAL_VALIDATE_SCHEMA(analyzer)


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({}, "at least one of rms, bands"),
        (
            {"bands": [{"band": 1, "name": "a"}, {"band": 1, "name": "b"}]},
            "Each band can only have one sensor",
        ),
    ],
)
def test_sensor_rejected(config: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        SENSOR_SCHEMA(
            {"audio_analyzer_id": ID(ANALYZER_ID, is_declaration=False), **config}
        )
