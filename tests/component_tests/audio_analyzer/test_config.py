"""Validation tests for audio_analyzer.

A `test*.yaml` can only show that a configuration is accepted, so these cover the rejections and the format the
speaker platform takes over from its output.
"""

import pytest

from esphome import config_validation as cv
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
    output = {CONF_ID: ID(OUTPUT_ID, is_declaration=True, type="speaker"), **output}
    mic = {
        CONF_ID: ID(MIC_ID, is_declaration=True, type="microphone"),
        "sample_rate": output.get("mic_sample_rate", 16000),
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


def _speaker_config(**options) -> ConfigType:
    return SPEAKER_CONFIG_SCHEMA(
        {
            CONF_ID: ID(ANALYZER_ID, is_declaration=True, type="speaker"),
            "output_speaker": ID(OUTPUT_ID, is_declaration=False),
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
            {"power_range": {"min_db": -20, "max_db": -20}},
            "min_db must be below max_db",
        ),
        ({"band_range": {"min_db": -10, "max_db": -30}}, "min_db must be below max_db"),
        (
            {"min_frequency": "3000Hz", "max_frequency": "2000Hz"},
            "min_frequency must be below max_frequency",
        ),
        ({"min_frequency": "20Hz", "max_frequency": "200Hz"}, "The lowest band is"),
        ({"max_frequency": "7000Hz"}, "max_frequency"),
    ],
)
def test_analysis_options_rejected(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _mic_config(**options)


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
    with pytest.raises(cv.Invalid, match="at most 3200 Hz for a 8000 Hz stream"):
        MIC_FINAL_VALIDATE_SCHEMA(_mic_config())


@pytest.mark.parametrize(
    ("options", "message"),
    [
        ({"buffer_duration": "0ms"}, "buffer_duration"),
        ({"buffer_duration": "50ms"}, "buffer_duration"),
        ({"audio_dac": "dac"}, "set audio_dac on the output speaker"),
        ({"sample_rate": 48000}, "set sample_rate on the output speaker"),
        ({"bits_per_sample": 16}, "set bits_per_sample on the output speaker"),
    ],
)
def test_speaker_options_rejected(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _speaker_config(**options)


def test_speaker_takes_output_format_and_limits(
    set_core_config: SetCoreConfigCallable,
) -> None:
    analyzer = _speaker_config()
    output = {
        "bits_per_sample": "passthrough",
        "sample_rate": 48000,
        "num_channels": 2,
        "min_bits_per_sample": 16,
        "max_bits_per_sample": 32,
        "min_sample_rate": 48000,
        "max_sample_rate": 48000,
    }
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(output, analyzer)
    )
    SPEAKER_FINAL_VALIDATE_SCHEMA(analyzer)
    assert "bits_per_sample" not in analyzer
    assert analyzer["sample_rate"] == 48000
    assert analyzer["num_channels"] == 2
    assert analyzer["min_bits_per_sample"] == 16
    assert analyzer["max_bits_per_sample"] == 32
    assert analyzer["min_sample_rate"] == 48000
    assert analyzer["max_sample_rate"] == 48000


def test_speaker_max_frequency_above_output_rate_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    analyzer = _speaker_config(max_frequency="4000Hz")
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config({"sample_rate": 8000}, analyzer),
    )
    with pytest.raises(cv.Invalid, match="at most 3200 Hz"):
        SPEAKER_FINAL_VALIDATE_SCHEMA(analyzer)


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({}, "at least one of power, bands"),
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
