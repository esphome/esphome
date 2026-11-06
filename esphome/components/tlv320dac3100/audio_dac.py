from dataclasses import dataclass
from fractions import Fraction

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_OUTPUT,
    CONF_RESET_PIN,
    CONF_SAMPLE_RATE,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_MCLK_FREQUENCY = "mclk_frequency"
CONF_SPEAKER_GAIN = "speaker_gain"

tlv320dac3100_ns = cg.esphome_ns.namespace("tlv320dac3100")
TLV320DAC3100 = tlv320dac3100_ns.class_(
    "TLV320DAC3100", AudioDac, cg.Component, i2c.I2CDevice
)

TLV320DAC3100Output = tlv320dac3100_ns.enum("TLV320DAC3100Output", is_class=True)
OUTPUTS = {
    "SPEAKER": TLV320DAC3100Output.SPEAKER,
    "HEADPHONE": TLV320DAC3100Output.HEADPHONE,
    "BOTH": TLV320DAC3100Output.BOTH,
}

# Page 1 / register 42 bits D4-D3
SPEAKER_GAINS = {"6dB": 0, "12dB": 1, "18dB": 2, "24dB": 3}

# I2S word length (page 0 / register 27 D5-D4) and BCLK periods per frame
BITS_PER_SAMPLE = {"16bit": (0, 32), "24bit": (2, 64), "32bit": (3, 64)}

# Clock limits from the datasheet (DVDD >= 1.65 V)
PLL_IN_MIN = 512_000
PLL_IN_MAX = 20_000_000
PLL_IN_FRACTIONAL_MIN = 10_000_000
PLL_OUT_MIN = 80_000_000
PLL_OUT_MAX = 110_000_000
CODEC_CLKIN_MAX = 110_000_000
DAC_CLK_MAX = 49_152_000
DAC_MOD_CLK_MAX = 6_758_000
DOSR_FS_MIN = 2_800_000
DOSR_FS_MAX = 6_200_000

# (max sample rate, processing block, interpolation, resource class)
PROCESSING_BLOCKS = (
    (48_000, 1, 8, 8),  # PRB_P1, filter A
    (96_000, 7, 4, 6),  # PRB_P7, filter B
    (192_000, 17, 2, 3),  # PRB_P17, filter C
)

# Page 0 / register 4: PLL_CLKIN and CODEC_CLKIN sources
CLKIN_MCLK = 0
CLKIN_BCLK = 1
CLKIN_PLL = 3


@dataclass(frozen=True)
class ClockConfig:
    mux: int
    pll_p: int
    pll_r: int
    pll_j: int
    pll_d: int
    ndac: int
    mdac: int
    dosr: int
    processing_block: int

    @property
    def uses_pll(self) -> bool:
        return self.mux & 0x03 == CLKIN_PLL


def _dividers(
    codec_clkin: int, sample_rate: int, interpolation: int, resource_class: int
) -> tuple[int, int, int] | None:
    """Pick NDAC, MDAC and DOSR, preferring the largest NDAC as the datasheet advises."""
    candidates = []
    dosr_min = -(-DOSR_FS_MIN // sample_rate)
    dosr_max = min(DOSR_FS_MAX // sample_rate, 1024)
    for dosr in range(dosr_min, dosr_max + 1):
        if dosr % interpolation or codec_clkin % (dosr * sample_rate):
            continue
        n_times_m = codec_clkin // (dosr * sample_rate)
        candidates.extend(
            (ndac, n_times_m // ndac, dosr)
            for ndac in range(1, min(n_times_m, 128) + 1)
            if n_times_m % ndac == 0
            and n_times_m // ndac <= 128
            and n_times_m // ndac * dosr >= 32 * resource_class
            and codec_clkin <= ndac * DAC_CLK_MAX
            and codec_clkin <= n_times_m * DAC_MOD_CLK_MAX
        )
    return max(candidates, key=lambda c: c[0], default=None)


def _pll(clkin: int, pll_out: int) -> tuple[int, int, int, int] | None:
    """Find P, R, J and D giving pll_out, preferring an integer multiplier."""
    candidates = []
    for p in range(1, 9):
        if not PLL_IN_MIN * p <= clkin <= PLL_IN_MAX * p:
            continue
        for r in range(1, 17):
            jd = Fraction(pll_out * p, clkin * r)
            j = int(jd)
            d = (jd - j) * 10_000
            if not 1 <= j <= 63 or d.denominator != 1:
                continue
            if d == 0 and not 4 <= r * j <= 259:
                continue
            if d != 0 and (r != 1 or clkin < PLL_IN_FRACTIONAL_MIN * p):
                continue
            candidates.append((p, r, j, int(d)))
    return min(candidates, key=lambda c: (c[3] != 0, c[0], c[1]), default=None)


def _processing_block(sample_rate: int) -> tuple[int, int, int]:
    for max_rate, prb, interpolation, resource_class in PROCESSING_BLOCKS:
        if sample_rate <= max_rate:
            return prb, interpolation, resource_class
    raise cv.Invalid(f"Sample rates above {max_rate}Hz are not supported")


def solve_clock(clkin: int, from_mclk: bool, sample_rate: int) -> ClockConfig:
    """Return clock dividers producing exactly sample_rate from clkin."""
    prb, interpolation, resource_class = _processing_block(sample_rate)
    source = CLKIN_MCLK if from_mclk else CLKIN_BCLK

    # MCLK straight into the dividers needs no PLL
    if (
        from_mclk
        and clkin <= CODEC_CLKIN_MAX
        and (divs := _dividers(clkin, sample_rate, interpolation, resource_class))
    ):
        return ClockConfig(source, 1, 1, 4, 0, *divs, prb)

    candidates = []
    step = sample_rate * interpolation
    for pll_out in range(-(-PLL_OUT_MIN // step) * step, PLL_OUT_MAX + 1, step):
        if (pll := _pll(clkin, pll_out)) is None:
            continue
        if divs := _dividers(pll_out, sample_rate, interpolation, resource_class):
            candidates.append((pll, divs))
    if not candidates:
        raise cv.Invalid(
            f"No valid clock settings for {sample_rate}Hz from a {clkin}Hz "
            f"{'MCLK' if from_mclk else 'BCLK'}"
        )
    # Integer PLL first, then the largest NDAC
    pll, divs = min(candidates, key=lambda c: (c[0][3] != 0, -c[1][0]))
    return ClockConfig((source << 2) | CLKIN_PLL, *pll, *divs, prb)


def _validate_clock(config: ConfigType) -> ConfigType:
    sample_rate = config[CONF_SAMPLE_RATE]
    if CONF_MCLK_FREQUENCY in config:
        solve_clock(config[CONF_MCLK_FREQUENCY], True, sample_rate)
    else:
        frame = BITS_PER_SAMPLE[config[CONF_BITS_PER_SAMPLE]][1]
        solve_clock(sample_rate * frame, False, sample_rate)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TLV320DAC3100),
            cv.Required(CONF_SAMPLE_RATE): cv.int_range(min=8000, max=192000),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.one_of(
                *BITS_PER_SAMPLE, lower=True
            ),
            cv.Optional(CONF_MCLK_FREQUENCY): cv.int_range(min=PLL_IN_MIN),
            cv.Optional(CONF_OUTPUT, default="BOTH"): cv.enum(OUTPUTS, upper=True),
            cv.Optional(CONF_SPEAKER_GAIN, default="6dB"): cv.enum(SPEAKER_GAINS),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18)),
    _validate_clock,
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sample_rate = config[CONF_SAMPLE_RATE]
    word_length, frame = BITS_PER_SAMPLE[config[CONF_BITS_PER_SAMPLE]]
    if (mclk := config.get(CONF_MCLK_FREQUENCY)) is not None:
        clock = solve_clock(mclk, True, sample_rate)
    else:
        clock = solve_clock(sample_rate * frame, False, sample_rate)

    cg.add(
        var.set_clock(
            clock.mux,
            clock.pll_p,
            clock.pll_r,
            clock.pll_j,
            clock.pll_d,
            clock.ndac,
            clock.mdac,
            clock.dosr,
            clock.processing_block,
        )
    )
    cg.add(var.set_word_length(word_length))
    cg.add(var.set_output(config[CONF_OUTPUT]))
    cg.add(var.set_speaker_gain(config[CONF_SPEAKER_GAIN]))
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
