"""Tests for the tlv320dac3100 clock divider solver."""

from __future__ import annotations

from fractions import Fraction

import pytest

from esphome.components.i2s_audio import (
    CONF_I2S_AUDIO_ID,
    CONF_I2S_MCLK_PIN,
    CONF_I2S_MODE,
    CONF_MCLK_MULTIPLE,
)
from esphome.components.i2s_audio.speaker import (
    CONF_DAC_TYPE,
    CONF_I2S_COMM_FMT,
    CONF_SPDIF_MODE,
)
from esphome.components.tlv320dac3100.audio_dac import (
    BITS_PER_SAMPLE,
    CLKIN_BCLK,
    CLKIN_MCLK,
    CLKIN_PLL,
    CODEC_CLKIN_MAX,
    CONF_MCLK_FREQUENCY,
    DAC_CLK_MAX,
    DAC_MOD_CLK_MAX,
    DOSR_FS_MAX,
    DOSR_FS_MIN,
    MCLK_IN_MAX,
    PLL_OUT_MAX,
    PLL_OUT_MIN,
    PROCESSING_BLOCKS,
    ClockConfig,
    _final_validate,
    _validate_mclk_frequency,
    solve_clock,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUDIO_DAC,
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_PLATFORM,
    CONF_SAMPLE_RATE,
)
import esphome.final_validate as fv

RATES = [8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000]


def _check(clock: ClockConfig, clkin: int, sample_rate: int) -> None:
    """Assert the settings meet every datasheet limit and give the exact rate."""
    if clock.uses_pll:
        pll_out = Fraction(
            clkin * clock.pll_r * (clock.pll_j * 10_000 + clock.pll_d),
            10_000 * clock.pll_p,
        )
        assert PLL_OUT_MIN <= pll_out <= PLL_OUT_MAX
        if clock.pll_d:
            assert clock.pll_r == 1
        else:
            assert 4 <= clock.pll_r * clock.pll_j <= 259
        codec_clkin = pll_out
    else:
        codec_clkin = Fraction(clkin)
    assert codec_clkin <= CODEC_CLKIN_MAX
    assert codec_clkin / clock.ndac <= DAC_CLK_MAX
    assert codec_clkin / (clock.ndac * clock.mdac) <= DAC_MOD_CLK_MAX
    assert DOSR_FS_MIN <= clock.dosr * sample_rate <= DOSR_FS_MAX
    assert codec_clkin / (clock.ndac * clock.mdac * clock.dosr) == sample_rate
    _, prb, interpolation, resource_class = next(
        block for block in PROCESSING_BLOCKS if block[1] == clock.processing_block
    )
    assert clock.dosr % interpolation == 0
    assert clock.mdac * clock.dosr >= 32 * resource_class


@pytest.mark.parametrize("sample_rate", RATES)
@pytest.mark.parametrize("frame", [32, 48, 64])
def test_bclk_clocks_are_within_limits(sample_rate: int, frame: int) -> None:
    """Every rate the BCLK can feed the PLL with meets the datasheet limits."""
    clkin = sample_rate * frame
    try:
        clock = solve_clock(clkin, False, sample_rate)
    except cv.Invalid:
        # Below the PLL's 512 kHz minimum input
        assert clkin < 512_000
        return
    assert clock.mux == (CLKIN_BCLK << 2) | CLKIN_PLL
    _check(clock, clkin, sample_rate)


@pytest.mark.parametrize(
    ("mclk", "sample_rate"),
    [
        (11_289_600, 44100),
        (12_288_000, 48000),
        (24_576_000, 48000),
        (12_000_000, 44100),
        (12_000_000, 48000),
        (19_200_000, 44100),
    ],
)
def test_mclk_clocks_are_within_limits(mclk: int, sample_rate: int) -> None:
    """Common MCLK frequencies meet the datasheet limits."""
    _check(solve_clock(mclk, True, sample_rate), mclk, sample_rate)


def test_mclk_multiple_skips_the_pll() -> None:
    """An MCLK that divides down to the rate drives the dividers directly."""
    clock = solve_clock(12_288_000, True, 48000)
    assert clock.mux == CLKIN_MCLK
    assert not clock.uses_pll


def test_non_multiple_mclk_uses_a_fractional_pll() -> None:
    """12 MHz needs a fractional multiplier for 44.1 kHz."""
    clock = solve_clock(12_000_000, True, 44100)
    assert clock.uses_pll
    assert clock.pll_d != 0


def test_bclk_too_slow_for_the_pll_is_rejected() -> None:
    """8 kHz with 16-bit slots gives a 256 kHz BCLK, below the PLL input range."""
    with pytest.raises(cv.Invalid, match="No valid clock settings"):
        solve_clock(256_000, False, 8000)


@pytest.mark.parametrize(
    ("sample_rate", "processing_block"), [(48000, 1), (96000, 7), (192000, 17)]
)
def test_processing_block_follows_the_rate(
    sample_rate: int, processing_block: int
) -> None:
    """Filter A up to 48 kHz, B up to 96 kHz, C above."""
    clock = solve_clock(sample_rate * 64, False, sample_rate)
    assert clock.processing_block == processing_block


def test_24_bit_frame_matches_the_i2s_speaker_slots() -> None:
    """The i2s_audio speaker clocks 24-bit audio in 24-bit slots."""
    assert BITS_PER_SAMPLE[24][1] == 48


def _speaker(platform: str = "i2s_audio", rate: int = 44100, bits: int = 16) -> dict:
    return {
        CONF_PLATFORM: platform,
        CONF_AUDIO_DAC: "dac",
        CONF_SAMPLE_RATE: rate,
        CONF_BITS_PER_SAMPLE: bits,
        CONF_DAC_TYPE: "external",
        CONF_I2S_COMM_FMT: "stand_i2s",
    }


_DAC = {CONF_ID: "dac", CONF_SAMPLE_RATE: 44100, CONF_BITS_PER_SAMPLE: 16}


def test_matching_i2s_speaker_is_accepted() -> None:
    fv.full_config.set({"speaker": [_speaker()]})
    _final_validate(_DAC)


@pytest.mark.parametrize(
    "speakers",
    [
        [],
        [{CONF_PLATFORM: "i2s_audio", CONF_SAMPLE_RATE: 44100}],
        [_speaker(platform="mixer")],
    ],
)
def test_dac_needs_a_direct_i2s_speaker(speakers: list[dict]) -> None:
    fv.full_config.set({"speaker": speakers})
    with pytest.raises(cv.Invalid, match="must set 'audio_dac: dac'"):
        _final_validate(_DAC)


@pytest.mark.parametrize("speaker", [_speaker(rate=48000), _speaker(bits=32)])
def test_mismatched_i2s_speaker_is_rejected(speaker: dict) -> None:
    fv.full_config.set({"speaker": [speaker]})
    with pytest.raises(cv.Invalid, match="must match"):
        _final_validate(_DAC)


_MCLK_DAC = {**_DAC, CONF_MCLK_FREQUENCY: 11_289_600}


def _mclk_speaker(mode: str = "primary", multiple: int = 256) -> dict:
    return {
        **_speaker(),
        CONF_I2S_AUDIO_ID: "bus",
        CONF_I2S_MODE: mode,
        CONF_MCLK_MULTIPLE: multiple,
    }


def _bus(has_mclk_pin: bool = True) -> dict:
    return {CONF_ID: "bus"} | ({CONF_I2S_MCLK_PIN: 15} if has_mclk_pin else {})


def test_mclk_matching_the_i2s_bus_is_accepted() -> None:
    fv.full_config.set({"speaker": [_mclk_speaker()], "i2s_audio": [_bus()]})
    _final_validate(_MCLK_DAC)


def test_mclk_not_matching_the_i2s_bus_is_rejected() -> None:
    fv.full_config.set(
        {"speaker": [_mclk_speaker(multiple=384)], "i2s_audio": [_bus()]}
    )
    with pytest.raises(cv.Invalid, match="outputs 16934400"):
        _final_validate(_MCLK_DAC)


@pytest.mark.parametrize(
    ("mode", "has_mclk_pin"), [("secondary", True), ("primary", False)]
)
def test_mclk_from_elsewhere_is_not_compared(mode: str, has_mclk_pin: bool) -> None:
    fv.full_config.set(
        {
            "speaker": [_mclk_speaker(mode=mode, multiple=384)],
            "i2s_audio": [_bus(has_mclk_pin)],
        }
    )
    _final_validate(_MCLK_DAC)


@pytest.mark.parametrize("fmt", ["stand_i2s", "stand_pcm_long", "i2s_msb", "pcm_long"])
def test_standard_i2s_formats_are_accepted(fmt: str) -> None:
    fv.full_config.set({"speaker": [{**_speaker(), CONF_I2S_COMM_FMT: fmt}]})
    _final_validate(_DAC)


@pytest.mark.parametrize(
    "fmt", ["stand_msb", "i2s_lsb", "stand_pcm_short", "pcm_short", "pcm"]
)
def test_non_standard_i2s_formats_are_rejected(fmt: str) -> None:
    fv.full_config.set({"speaker": [{**_speaker(), CONF_I2S_COMM_FMT: fmt}]})
    with pytest.raises(cv.Invalid, match="needs standard I2S"):
        _final_validate(_DAC)


def test_spdif_speaker_is_rejected() -> None:
    fv.full_config.set({"speaker": [{**_speaker(), CONF_SPDIF_MODE: True}]})
    with pytest.raises(cv.Invalid, match="spdif_mode"):
        _final_validate(_DAC)


def test_internal_dac_speaker_is_rejected() -> None:
    fv.full_config.set({"speaker": [{**_speaker(), CONF_DAC_TYPE: "internal"}]})
    with pytest.raises(cv.Invalid, match="dac_type: external"):
        _final_validate(_DAC)


def test_mclk_is_capped_at_the_datasheet_limit() -> None:
    assert _validate_mclk_frequency(MCLK_IN_MAX) == MCLK_IN_MAX
    with pytest.raises(cv.Invalid):
        _validate_mclk_frequency(MCLK_IN_MAX + 1)
