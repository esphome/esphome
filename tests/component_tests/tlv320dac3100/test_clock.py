"""Tests for the tlv320dac3100 clock divider solver."""

from __future__ import annotations

from fractions import Fraction

import pytest

from esphome.components.tlv320dac3100.audio_dac import (
    CLKIN_BCLK,
    CLKIN_MCLK,
    CLKIN_PLL,
    CODEC_CLKIN_MAX,
    DAC_CLK_MAX,
    DAC_MOD_CLK_MAX,
    DOSR_FS_MAX,
    DOSR_FS_MIN,
    PLL_OUT_MAX,
    PLL_OUT_MIN,
    PROCESSING_BLOCKS,
    ClockConfig,
    solve_clock,
)
import esphome.config_validation as cv

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
@pytest.mark.parametrize("frame", [32, 64])
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
