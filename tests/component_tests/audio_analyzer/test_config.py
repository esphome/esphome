"""Validation tests for audio_analyzer.

A `test*.yaml` can only show that a configuration is accepted, so these cover the rejections and how the speaker
platform's declared format is checked against its output and by the components that write to it.
"""

import cmath
import math

import pytest

from esphome import config_validation as cv
from esphome.components import audio
from esphome.components.audio_analyzer import (
    CONFIG_SCHEMA as MIC_CONFIG_SCHEMA,
    FFT_SIZE,
    FINAL_VALIDATE_SCHEMA as MIC_FINAL_VALIDATE_SCHEMA,
    MAX_BAND_FRACTION,
    MIN_BAND_BINS,
    NUM_BANDS,
    analysis_plan,
    analysis_rate,
    anti_alias_sections,
    band_bins,
    smoothing_factor,
)
from esphome.components.audio_analyzer.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA
from esphome.components.audio_analyzer.speaker import (
    CONFIG_SCHEMA as SPEAKER_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as SPEAKER_FINAL_VALIDATE_SCHEMA,
    output_latency_ms,
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
        "min_sample_rate": mic_sample_rate,
        "max_sample_rate": output.pop("mic_max_sample_rate", mic_sample_rate),
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
            "raise max_frequency to at least 766 Hz",
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
    assert bins[0] >= 1
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


def test_speaker_bands_checked_against_its_rate() -> None:
    with pytest.raises(cv.Invalid, match="at most 2400 Hz for a 8000 Hz stream"):
        _speaker_config(sample_rate=8000, max_frequency="4000Hz")
    # Without max_frequency, the default is lowered to what the rate allows
    _speaker_config(sample_rate=22050)


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


def test_default_buffer_covers_the_output_chain(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # analyzer -> resampler (100 ms) -> mixer source (100 ms, output_speaker set by the mixer) -> i2s (500 ms)
    full = Config()
    blocks = [
        {"id": "resampler", "output_speaker": "source", "buffer_duration": 100},
        {"id": "source", "output_speaker": "i2s", "buffer_duration": 100},
        {"id": "i2s", "buffer_duration": 500},
    ]
    full["speaker"] = []
    for index, block in enumerate(blocks):
        entry = {
            CONF_ID: ID(block["id"], is_declaration=True, type="speaker"),
            "buffer_duration": cv.TimePeriod(milliseconds=block["buffer_duration"]),
        }
        if "output_speaker" in block:
            entry["output_speaker"] = ID(block["output_speaker"], is_declaration=False)
        full["speaker"].append(entry)
        full.declare_ids.append((entry[CONF_ID], ["speaker", index, CONF_ID]))
    analyzer = {"output_speaker": ID("resampler", is_declaration=False)}
    assert output_latency_ms(analyzer, full) == 700
