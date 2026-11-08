from dataclasses import dataclass
from fractions import Fraction
from functools import cache

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
from esphome.components.i2s_audio import (
    CONF_I2S_AUDIO_ID,
    CONF_I2S_COMM_FMT,
    CONF_I2S_MCLK_PIN,
    CONF_I2S_MODE,
    CONF_MCLK_MULTIPLE,
    CONF_PRIMARY,
    DOMAIN as I2S_AUDIO_DOMAIN,
)
from esphome.components.speaker import DOMAIN as SPEAKER_DOMAIN, get_audio_dac_ids
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_OUTPUT,
    CONF_PLATFORM,
    CONF_RESET_PIN,
    CONF_SAMPLE_RATE,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_MCLK_FREQUENCY = "mclk_frequency"
CONF_SPEAKER_GAIN = "speaker_gain"

tlv320dac3100_ns = cg.esphome_ns.namespace("tlv320dac3100")
TLV320DAC3100 = tlv320dac3100_ns.class_(
    "TLV320DAC3100", AudioDac, cg.Component, i2c.I2CDevice
)
I2CRegValue = i2c.i2c_ns.struct("I2CRegValue")

# Page 1 / register 42 bits D4-D3
SPEAKER_GAINS = {6.0: 0, 12.0: 1, 18.0: 2, 24.0: 3}

# I2S word length, page 0 / register 27 D5-D4
WORD_LENGTHS = {16: 0, 24: 2, 32: 3}

_validate_bits = cv.float_with_unit("bits", "bit")

# Clock limits from the datasheet (DVDD >= 1.65 V)
MCLK_IN_MAX = 50_000_000
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

# i2s_comm_fmt values the speaker clocks as standard I2S, the DAC's fixed mode
STANDARD_I2S_FORMATS = ("stand_i2s", "stand_pcm_long", "i2s_msb", "pcm_long")

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


@cache
def solve_clock(clkin: int, from_mclk: bool, sample_rate: int) -> ClockConfig:
    """Return clock dividers producing exactly sample_rate from clkin."""
    _, prb, interpolation, resource_class = next(
        b for b in PROCESSING_BLOCKS if sample_rate <= b[0]
    )
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


def _solve_config_clock(config: ConfigType) -> ClockConfig:
    sample_rate = config[CONF_SAMPLE_RATE]
    if (mclk := config.get(CONF_MCLK_FREQUENCY)) is not None:
        return solve_clock(mclk, True, sample_rate)
    # Two slots of bits_per_sample each
    return solve_clock(
        sample_rate * 2 * config[CONF_BITS_PER_SAMPLE], False, sample_rate
    )


def _validate_clock(config: ConfigType) -> ConfigType:
    _solve_config_clock(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TLV320DAC3100),
            cv.Required(CONF_SAMPLE_RATE): cv.int_range(min=8000, max=192000),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                _validate_bits, cv.int_, cv.one_of(*WORD_LENGTHS)
            ),
            cv.Optional(CONF_MCLK_FREQUENCY): cv.int_range(
                min=PLL_IN_MIN, max=MCLK_IN_MAX
            ),
            cv.Optional(CONF_OUTPUT, default="BOTH"): cv.one_of(
                "SPEAKER", "HEADPHONE", "BOTH", upper=True
            ),
            cv.Optional(CONF_SPEAKER_GAIN, default="6dB"): cv.All(
                cv.decibel, cv.enum(SPEAKER_GAINS)
            ),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18)),
    _validate_clock,
)


def _generated_mclk(speaker: ConfigType) -> int | None:
    """Return the MCLK the speaker's I2S bus outputs, or None if it drives no MCLK."""
    if speaker[CONF_I2S_MODE] != CONF_PRIMARY:
        return None
    for bus in fv.full_config.get().get(I2S_AUDIO_DOMAIN, []):
        if bus[CONF_ID] == speaker[CONF_I2S_AUDIO_ID] and CONF_I2S_MCLK_PIN in bus:
            return speaker[CONF_SAMPLE_RATE] * speaker[CONF_MCLK_MULTIPLE]
    return None


def _final_validate(config: ConfigType) -> None:
    # Only the i2s_audio speaker reports that its clocks are running
    speakers = [
        speaker
        for speaker in fv.full_config.get().get(SPEAKER_DOMAIN, [])
        if speaker[CONF_PLATFORM] == "i2s_audio"
        and config[CONF_ID] in get_audio_dac_ids(speaker)
    ]
    if not speakers:
        raise cv.Invalid(
            f"An i2s_audio speaker must set 'audio_dac: {config[CONF_ID]}'; "
            "the DAC powers up when that speaker starts its clocks"
        )
    for speaker in speakers:
        fmt = speaker.get(CONF_I2S_COMM_FMT, "stand_i2s")
        if fmt not in STANDARD_I2S_FORMATS:
            raise cv.Invalid(
                f"The i2s_audio speaker using this audio_dac has i2s_comm_fmt {fmt}; "
                "the DAC needs standard I2S"
            )
        for key in (CONF_SAMPLE_RATE, CONF_BITS_PER_SAMPLE):
            if speaker[key] != config[key]:
                raise cv.Invalid(
                    f"{key} is {config[key]} but the i2s_audio speaker using this "
                    f"audio_dac has {speaker[key]}; they must match"
                )
        if (mclk := config.get(CONF_MCLK_FREQUENCY)) is not None and (
            generated := _generated_mclk(speaker)
        ) not in (None, mclk):
            raise cv.Invalid(
                f"{CONF_MCLK_FREQUENCY} is {mclk} but the i2s_audio speaker using "
                f"this audio_dac outputs {generated} ({CONF_SAMPLE_RATE} x "
                f"{CONF_MCLK_MULTIPLE})"
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    clock = _solve_config_clock(config)
    headphone = config[CONF_OUTPUT] != "SPEAKER"
    speaker = config[CONF_OUTPUT] != "HEADPHONE"
    gain = SPEAKER_GAINS[config[CONF_SPEAKER_GAIN]] << 3
    setup = (
        # Page 0, selected by the reset
        (0x04, clock.mux),
        (0x06, clock.pll_j),
        # D MSB must be written immediately before D LSB
        (0x07, clock.pll_d >> 8),
        (0x08, clock.pll_d & 0xFF),
        # PLL power, P and R (8 and 16 are written as 0)
        (0x05, clock.uses_pll << 7 | (clock.pll_p & 0x07) << 4 | clock.pll_r & 0x0F),
        # NDAC and MDAC powered up; 128 (and DOSR 1024) are written as 0
        (0x0B, 0x80 | clock.ndac & 0x7F),
        (0x0C, 0x80 | clock.mdac & 0x7F),
        (0x0D, clock.dosr >> 8 & 0x03),
        (0x0E, clock.dosr & 0xFF),
        (0x1B, WORD_LENGTHS[config[CONF_BITS_PER_SAMPLE]] << 4),
        (0x3C, clock.processing_block),
        # Page 1; drivers stay muted until the DAC is up
        (0x00, 0x01),
        (0x21, 0x4E),  # HP driver power-on 1.22 s, 3.9 ms steps
        (0x23, 0x44),  # DAC_L and DAC_R to the mixer amplifiers
        (0x28, 0x02),  # HPL and HPR at 0 dB, muted
        (0x29, 0x02),
        (0x2A, gain),
        (0x1F, 0xC4 if headphone else 0x04),  # 1.35 V common mode
        (0x20, 0x86 if speaker else 0x06),
        # Analog volume routed at 0 dB; level is set digitally
        (0x24, 0x80),
        (0x25, 0x80),
        (0x26, 0x80),
        (0x00, 0x00),
    )
    table = cg.shared_progmem_array(
        "tlv320dac3100_setup",
        I2CRegValue,
        cg.ArrayInitializer(
            *(cg.RawExpression(f"{{0x{r:02X}, 0x{v:02X}}}") for r, v in setup),
            multiline=True,
        ),
    )
    var = await i2c.new_i2c_component(
        config,
        table,
        len(setup),
        # DAC data path: both channels on; the mono speaker gets (L + R) / 2
        0xD4 if headphone else 0xF4,
        # Output drivers unmuted (D2)
        0x06 if headphone else 0x02,
        gain | speaker << 2,
    )
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
