"""Validation tests for audio_analyzer.

A `test*.yaml` can only show that a configuration is accepted, so these cover the rejections and the band plan and
filter design.
"""

import cmath
import math

import pytest

from esphome import config_validation as cv
from esphome.components.audio_analyzer import (
    CONFIG_SCHEMA as MIC_CONFIG_SCHEMA,
    FFT_SIZE,
    FINAL_VALIDATE_SCHEMA as MIC_FINAL_VALIDATE_SCHEMA,
    MAX_BAND_FRACTION,
    MIN_BAND_BIN,
    MIN_BAND_BINS,
    NUM_BANDS,
    analysis_plan,
    analysis_rate,
    anti_alias_sections,
    band_bins,
    smoothing_factor,
)
from esphome.components.audio_analyzer.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.config import Config
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

MIC_ID = "mic"
ANALYZER_ID = "analyzer"


def _full_config(mic: ConfigType) -> Config:
    """A full config with one microphone, as the ID pass leaves it."""
    full = Config()
    sample_rate = mic.get("mic_sample_rate", 16000)
    full["microphone"] = [
        {
            CONF_ID: ID(MIC_ID, is_declaration=True, type="microphone"),
            "min_sample_rate": sample_rate,
            "max_sample_rate": mic.get("mic_max_sample_rate", sample_rate),
            "min_channels": 1,
            "max_channels": 1,
        }
    ]
    full.declare_ids.append(
        (full["microphone"][0][CONF_ID], ["microphone", 0, CONF_ID])
    )
    return full


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


def _validate_mic(**options) -> None:
    MIC_FINAL_VALIDATE_SCHEMA(_mic_config(**options))


@pytest.mark.parametrize(
    ("options", "message"),
    [
        (
            {"rms_range": {"min_db": -20, "max_db": -20}},
            "min_db must be below max_db",
        ),
        ({"band_range": {"min_db": -10, "max_db": -30}}, "min_db must be below max_db"),
        ({"max_frequency": "4900Hz"}, "value must be at most 4800"),
    ],
)
def test_analysis_options_rejected(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _mic_config(**options)


@pytest.mark.parametrize(
    ("options", "message"),
    [
        (
            {"min_frequency": "3000Hz", "max_frequency": "2000Hz"},
            "min_frequency must be below max_frequency",
        ),
        (
            {"min_frequency": "20Hz", "max_frequency": "200Hz"},
            "raise max_frequency to at least 797 Hz",
        ),
        (
            {"min_frequency": "4000Hz", "max_frequency": "4700Hz"},
            "raise max_frequency to at least 4735 Hz",
        ),
    ],
)
def test_band_range_rejected_for_the_rate(options: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        _validate_mic(**options)


@pytest.mark.parametrize(
    ("min_frequency", "max_frequency"),
    [("20Hz", "800Hz"), ("4000Hz", "4800Hz"), ("100Hz", "4800Hz")],
)
def test_narrowest_ranges_accepted(min_frequency: str, max_frequency: str) -> None:
    _validate_mic(min_frequency=min_frequency, max_frequency=max_frequency)


def test_band_plan_leaves_out_the_bins_dc_reaches() -> None:
    # The Hann window spreads DC into bins 0 and 1; a lower min_frequency still starts at bin 2 (the C++ tests use this
    # plan as LOW_PLAN)
    assert band_bins(20, 4000, 16000) == [
        2,
        4,
        6,
        8,
        10,
        12,
        16,
        23,
        32,
        45,
        64,
        91,
        128,
    ]


def test_default_band_plan() -> None:
    # The plan the C++ tests use as DEFAULT_PLAN
    plan = analysis_plan(_mic_config(), 16000)
    assert plan.bins == [3, 5, 7, 9, 11, 14, 20, 27, 37, 50, 68, 94, 128]
    assert (plan.factor, plan.rate) == (1, 16000)


@pytest.mark.parametrize(
    ("sample_rate", "expected"),
    [(8000, 8000), (16000, 16000), (22050, 11025), (44100, 14700), (48000, 16000)],
)
def test_analysis_rate(sample_rate: int, expected: float) -> None:
    assert analysis_rate(sample_rate) == expected


@pytest.mark.parametrize(
    "sample_rate", [8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000]
)
@pytest.mark.parametrize(
    ("min_frequency", "max_frequency"),
    [(100, None), (20, 800), (100, 4800), (1000, 4800)],
)
def test_band_plan_at_every_rate(
    sample_rate: int, min_frequency: int, max_frequency: int | None
) -> None:
    rate = analysis_rate(sample_rate)
    limit = math.floor(MAX_BAND_FRACTION * rate)
    top = min(max_frequency or 4000, limit)
    try:
        bins = band_bins(min_frequency, top, rate)
    except cv.Invalid:
        # Only when the rate's bins are too wide for the range
        assert (
            round(top / (rate / FFT_SIZE)) - round(min_frequency / (rate / FFT_SIZE))
            < 2 * NUM_BANDS
        )
        return
    assert len(bins) == NUM_BANDS + 1
    assert bins[0] >= MIN_BAND_BIN
    assert all(b - a >= MIN_BAND_BINS for a, b in zip(bins, bins[1:], strict=False))
    assert (bins[-1] - 0.5) * rate / FFT_SIZE <= limit


def test_default_max_frequency_follows_low_rates() -> None:
    # 4000 Hz doesn't fit an 8 kHz stream; the default becomes its limit instead of an error
    assert analysis_plan(_mic_config(), 8000).bins[-1] == round(2400 / 15.625)
    with pytest.raises(cv.Invalid, match="at most 2400 Hz for a 8000 Hz stream"):
        analysis_plan(_mic_config(max_frequency="4000Hz"), 8000)


def _filter_gain_db(factor: int, fraction: float) -> float:
    """Gain of the anti-alias filter at a frequency given as a fraction of the input rate."""
    z1 = cmath.exp(-2j * math.pi * fraction)
    gain = 1.0
    for b0, a1, a2 in anti_alias_sections(factor):
        gain *= abs(b0 * (1 + 2 * z1 + z1 * z1) / (1 + a1 * z1 + a2 * z1 * z1))
    return 20 * math.log10(max(gain, 1e-12))


@pytest.mark.parametrize("sample_rate", [22050, 24000, 32000, 44100, 48000])
def test_filter_passband_is_flat_up_to_the_top_band(sample_rate: int) -> None:
    factor = analysis_plan(_mic_config(), sample_rate).factor
    top = MAX_BAND_FRACTION * sample_rate / factor
    assert _filter_gain_db(factor, top / sample_rate) > -0.1


@pytest.mark.parametrize(
    ("sample_rate", "worst_db"),
    [(22050, 69), (24000, 69), (32000, 69), (44100, 48.9), (48000, 48.9)],
)
def test_filter_attenuates_every_alias_of_the_bands(
    sample_rate: int, worst_db: float
) -> None:
    # Every input frequency that folds into 0 Hz..top band edge, on a 1 Hz grid
    factor = analysis_plan(_mic_config(), sample_rate).factor
    out = sample_rate / factor
    top = MAX_BAND_FRACTION * out
    worst = max(
        _filter_gain_db(factor, frequency / sample_rate)
        for frequency in range(math.floor(out - top), sample_rate // 2 + 1)
        if min(frequency % out, out - frequency % out) <= top
    )
    assert worst < -45
    assert worst == pytest.approx(-worst_db, abs=0.1)


def test_smoothing_factor() -> None:
    # 63 % of a step after one time constant: one 32 ms frame of a 32 ms attack
    assert smoothing_factor(32, 16000) == pytest.approx(1 - math.exp(-1))
    assert smoothing_factor(0, 16000) == 1.0


@pytest.mark.parametrize(
    ("mic", "message"),
    [
        ({"mic_sample_rate": None}, "one fixed sample rate"),
        (
            {"mic_sample_rate": 16000, "mic_max_sample_rate": 48000},
            "one fixed sample rate",
        ),
        ({"mic_sample_rate": 96000}, "above 48000 Hz is not supported"),
        ({"mic_sample_rate": 8000}, "at most 2400 Hz for a 8000 Hz stream"),
    ],
)
def test_microphone_rate_rejected(
    set_core_config: SetCoreConfigCallable, mic: ConfigType, message: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(mic))
    with pytest.raises(cv.Invalid, match=message):
        _validate_mic(max_frequency="4000Hz")


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({}, "at least one of rms, bands"),
        (
            {"bands": [{"band": 1, "name": "a"}, {"band": 1, "name": "b"}]},
            "Each band can only have one sensor",
        ),
        ({"bands": []}, "length of value must be at least 1"),
    ],
)
def test_sensor_rejected(config: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        SENSOR_SCHEMA(
            {"audio_analyzer_id": ID(ANALYZER_ID, is_declaration=False), **config}
        )
