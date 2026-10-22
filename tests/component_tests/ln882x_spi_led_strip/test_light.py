"""Tests for the ln882x_spi_led_strip SPI encoding and validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_CHANNEL_COLORS
from esphome.components.ln882x_spi_led_strip.light import (
    CHIPSETS,
    SPIEncoding,
    _final_validate,
    _validate_num_leds,
    spi_encoding,
)
from esphome.const import CONF_CHIPSET, CONF_NUM_LEDS, CONF_PLATFORM, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

# Datasheets allow +-150 ns on the high times; APA106 bit 1 is 160 ns short (1200 vs 1360 ns).
HIGH_TOLERANCE_NS = 160
# Every low time stays at least this long, and the bit period well below the reset time.
MIN_LOW_NS = 400
MAX_BIT_PERIOD_NS = 5000


def _pulses(encoding: SPIEncoding, high_bits: int) -> tuple[int, int]:
    """Decode an SPI byte with high_bits leading 1-bits into (high, low) ns."""
    bit_ns = 25 << encoding.prescaler  # 40 MHz APB0
    pattern = (0xFF00 >> high_bits) & 0xFF
    high = pattern.bit_count()
    assert pattern == (0xFF << (8 - high)) & 0xFF, "1-bits must lead"
    return high * bit_ns, (8 - high) * bit_ns


@pytest.mark.parametrize("chipset", CHIPSETS)
def test_spi_encoding_meets_chipset_timings(chipset: str) -> None:
    timings = CHIPSETS[chipset]
    encoding = spi_encoding(timings)
    high0, low0 = _pulses(encoding, encoding.bit0_high_bits)
    high1, low1 = _pulses(encoding, encoding.bit1_high_bits)

    assert abs(high0 - timings.bit0_high) <= HIGH_TOLERANCE_NS
    assert abs(high1 - timings.bit1_high) <= HIGH_TOLERANCE_NS
    assert high1 > high0
    assert min(low0, low1) >= MIN_LOW_NS
    assert high0 + low0 <= MAX_BIT_PERIOD_NS
    # The reset gap is at least 300 us.
    assert encoding.reset_bytes * 8 * (25 << encoding.prescaler) >= 300_000


def test_spi_encoding_values() -> None:
    # SM16703 is hardware-tested at 5 MHz with 400 / 1000 ns high times.
    assert spi_encoding(CHIPSETS["SM16703"]) == SPIEncoding(3, 2, 5, 188)
    assert spi_encoding(CHIPSETS["APA106"]) == SPIEncoding(3, 2, 6, 188)


@pytest.mark.parametrize(
    ("channel_colors", "max_leds"), [("GRB", 2722), ("GRBW", 2042)]
)
def test_num_leds_limit(channel_colors: str, max_leds: int) -> None:
    def config(num_leds: int) -> dict:
        return {
            CONF_CHIPSET: "WS2812",
            CONF_CHANNEL_COLORS: channel_colors,
            CONF_NUM_LEDS: num_leds,
        }

    _validate_num_leds(config(max_leds))
    with pytest.raises(cv.Invalid, match=f"maximum number of LEDs .* is {max_leds}"):
        _validate_num_leds(config(max_leds + 1))


@pytest.mark.parametrize(("count", "fails"), [(1, False), (2, True)])
def test_single_instance(
    set_core_config: SetCoreConfigCallable, count: int, fails: bool
) -> None:
    lights = [{CONF_PLATFORM: "ln882x_spi_led_strip"}] * count + [
        {CONF_PLATFORM: "binary"}
    ]
    set_core_config(PlatformFramework.LN882X_ARDUINO, full_config={"light": lights})
    if fails:
        with pytest.raises(cv.Invalid, match="Only one ln882x_spi_led_strip"):
            _final_validate({})
    else:
        _final_validate({})
