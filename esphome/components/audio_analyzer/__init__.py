"""Live RMS level and frequency spectrum of an audio stream."""

from dataclasses import dataclass
import math

from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MICROPHONE, PLATFORM_ESP32
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["audio", "ring_buffer"]
CODEOWNERS = ["@jlpouffier"]
DOMAIN = "audio_analyzer"
MULTI_CONF = True
# Auto-loaded by the speaker platform without any microphone analyzer
MULTI_CONF_NO_DEFAULT = True

CONF_ATTACK = "attack"
CONF_AUDIO_ANALYZER_ID = "audio_analyzer_id"
CONF_BAND_RANGE = "band_range"
CONF_MAX_DB = "max_db"
CONF_MAX_FREQUENCY = "max_frequency"
CONF_MIN_DB = "min_db"
CONF_MIN_FREQUENCY = "min_frequency"
CONF_PASSIVE = "passive"
CONF_RELEASE = "release"
CONF_RMS_RANGE = "rms_range"

# Must match analysis.h (checked by a static_assert in the generated code)
FFT_SIZE = 512
NUM_BANDS = 12
NUM_SECTIONS = 4
# Streams are decimated by an integer factor to at most this rate
MAX_ANALYSIS_RATE = 16000
# The anti-alias filter holds up to a decimation factor of 3
MAX_SAMPLE_RATE = 48000
# Highest band edge and anti-alias corner, as fractions of the analysis rate
MAX_BAND_FRACTION = 0.3
FILTER_CORNER_FRACTION = 0.4
# Narrowest band (the Hann main lobe) and lowest band bin (above the DC leakage), in FFT bins
MIN_BAND_BINS = 2
MIN_BAND_BIN = 2
DEFAULT_MAX_FREQUENCY = 4000
MICROPHONE_BUFFER_MS = 120

audio_analyzer_ns = cg.esphome_ns.namespace("audio_analyzer")
AudioAnalyzer = audio_analyzer_ns.class_("AudioAnalyzer", cg.Component)
MicrophoneAudioAnalyzer = audio_analyzer_ns.class_(
    "MicrophoneAudioAnalyzer", AudioAnalyzer
)


def decimation_factor(sample_rate: int) -> int:
    return max(1, math.ceil(sample_rate / MAX_ANALYSIS_RATE))


def analysis_rate(sample_rate: int) -> float:
    return sample_rate / decimation_factor(sample_rate)


def anti_alias_sections(factor: int) -> list[tuple[float, float, float]]:
    """8th-order Butterworth low-pass as biquads (RBJ cookbook), each ``(b0, a1, a2)`` normalized by a0; the
    numerator is b0 (1 + 2 z^-1 + z^-2)."""
    w0 = 2 * math.pi * FILTER_CORNER_FRACTION / factor
    sections = []
    for k in range(NUM_SECTIONS):
        q = 1 / (2 * math.cos(math.pi * (2 * k + 1) / (4 * NUM_SECTIONS)))
        alpha = math.sin(w0) / (2 * q)
        a0 = 1 + alpha
        sections.append(
            ((1 - math.cos(w0)) / 2 / a0, -2 * math.cos(w0) / a0, (1 - alpha) / a0)
        )
    return sections


def _round_bin(frequency: float, bin_width: float) -> int:
    return math.floor(frequency / bin_width + 0.5)


def band_bins(min_frequency: float, max_frequency: float, rate: float) -> list[int]:
    """First FFT bin of each band, then the end of the last band: log-spaced, at least ``MIN_BAND_BINS`` wide."""
    bin_width = rate / FFT_SIZE
    lower = max(MIN_BAND_BIN, _round_bin(min_frequency, bin_width))
    upper = _round_bin(max_frequency, bin_width)
    if upper - lower < NUM_BANDS * MIN_BAND_BINS:
        needed = (lower + NUM_BANDS * MIN_BAND_BINS - 0.5) * bin_width
        raise cv.Invalid(
            f"The range is too narrow for {NUM_BANDS} bands of at least {MIN_BAND_BINS * bin_width:g} Hz "
            f"({MIN_BAND_BINS} analysis bins at {rate:g} Hz): raise {CONF_MAX_FREQUENCY} to at least "
            f"{math.ceil(needed)} Hz or lower {CONF_MIN_FREQUENCY}",
            path=[CONF_MAX_FREQUENCY],
        )
    bins = [lower]
    for band in range(1, NUM_BANDS):
        edge = lower * (upper / lower) ** (band / NUM_BANDS)
        bins.append(max(_round_bin(edge, 1), bins[-1] + MIN_BAND_BINS))
    bins.append(upper)
    for band in range(NUM_BANDS - 1, 0, -1):
        bins[band] = min(bins[band], bins[band + 1] - MIN_BAND_BINS)
    return bins


@dataclass
class AudioAnalyzerData:
    constants_checked: bool = False


def _get_data() -> AudioAnalyzerData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = AudioAnalyzerData()
    return CORE.data[DOMAIN]


@dataclass(frozen=True)
class AnalysisPlan:
    sample_rate: int
    factor: int
    rate: float
    bins: list[int]


def analysis_plan(config: ConfigType, sample_rate: int) -> AnalysisPlan:
    """Band plan for a stream rate; rejects bands the rate can't hold."""
    if sample_rate > MAX_SAMPLE_RATE:
        raise cv.Invalid(
            f"Audio above {MAX_SAMPLE_RATE} Hz is not supported ({sample_rate} Hz)"
        )
    rate = analysis_rate(sample_rate)
    limit = math.floor(MAX_BAND_FRACTION * rate)
    max_frequency = config.get(CONF_MAX_FREQUENCY, min(DEFAULT_MAX_FREQUENCY, limit))
    if max_frequency > limit:
        raise cv.Invalid(
            f"{CONF_MAX_FREQUENCY} must be at most {limit} Hz for a {sample_rate} Hz stream",
            path=[CONF_MAX_FREQUENCY],
        )
    if config[CONF_MIN_FREQUENCY] >= max_frequency:
        raise cv.Invalid(
            f"{CONF_MIN_FREQUENCY} must be below {CONF_MAX_FREQUENCY} ({max_frequency} Hz)",
            path=[CONF_MIN_FREQUENCY],
        )
    return AnalysisPlan(
        sample_rate,
        decimation_factor(sample_rate),
        rate,
        band_bins(config[CONF_MIN_FREQUENCY], max_frequency, rate),
    )


def smoothing_factor(time_constant_ms: int, rate: float) -> float:
    """Share of a step applied per analysis frame, 1 for no smoothing."""
    if time_constant_ms == 0:
        return 1.0
    return 1 - math.exp(-FFT_SIZE / rate * 1000 / time_constant_ms)


def _validate_db_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_DB] >= config[CONF_MAX_DB]:
        raise cv.Invalid(f"{CONF_MIN_DB} must be below {CONF_MAX_DB}")
    return config


def _db_range_schema(min_default: float, max_default: float) -> cv.All:
    db = cv.float_range(min=-100, max=0)
    return cv.All(
        cv.Schema(
            {
                cv.Optional(CONF_MIN_DB, default=min_default): db,
                cv.Optional(CONF_MAX_DB, default=max_default): db,
            }
        ),
        _validate_db_range,
    )


# Shared by every analyzer; analysis_plan() checks the band range against the stream rate
ANALYZER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_FREQUENCY, default="100Hz"): cv.All(
            cv.frequency, cv.Range(min=20, max=4000)
        ),
        # Defaults to DEFAULT_MAX_FREQUENCY, or lower when the stream needs it
        cv.Optional(CONF_MAX_FREQUENCY): cv.All(
            cv.frequency,
            cv.Range(min=100, max=MAX_BAND_FRACTION * MAX_ANALYSIS_RATE),
        ),
        cv.Optional(CONF_RMS_RANGE, default={}): _db_range_schema(-70, -20),
        cv.Optional(CONF_BAND_RANGE, default={}): _db_range_schema(-80, -30),
        cv.Optional(CONF_ATTACK, default="30ms"): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_RELEASE, default="250ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


def _float(value: float) -> str:
    return f"{value!r}f"


async def setup_analyzer(
    var: cg.MockObj, config: ConfigType, sample_rate: int, buffer_ms: int
) -> None:
    await cg.register_component(var, config)
    plan = analysis_plan(config, sample_rate)
    if not (data := _get_data()).constants_checked:
        data.constants_checked = True
        cg.add_global(
            cg.RawStatement(
                f"static_assert(audio_analyzer::FFT_SIZE == {FFT_SIZE} && audio_analyzer::NUM_BANDS == {NUM_BANDS} && "
                f"audio_analyzer::Decimator::NUM_SECTIONS == {NUM_SECTIONS}, "
                '"audio_analyzer: FFT_SIZE, NUM_BANDS or NUM_SECTIONS differ between __init__.py and analysis.h");'
            )
        )
    bins = ", ".join(str(b) for b in plan.bins)
    cg.add(
        var.set_band_plan(
            plan.rate,
            cg.RawExpression(f"std::array<uint16_t, {NUM_BANDS + 1}>{{{bins}}}"),
        )
    )
    if plan.factor > 1:
        sections = ", ".join(
            f"{{{', '.join(_float(c) for c in section)}}}"
            for section in anti_alias_sections(plan.factor)
        )
        cg.add(
            var.set_decimation(
                plan.factor,
                cg.RawExpression(
                    "std::array<audio_analyzer::Decimator::Section, audio_analyzer::Decimator::NUM_SECTIONS>"
                    f"{{{{{sections}}}}}"
                ),
            )
        )
    cg.add(
        var.set_smoothing(
            smoothing_factor(config[CONF_ATTACK].total_milliseconds, plan.rate),
            smoothing_factor(config[CONF_RELEASE].total_milliseconds, plan.rate),
        )
    )
    rms_range = config[CONF_RMS_RANGE]
    cg.add(var.set_rms_range(rms_range[CONF_MIN_DB], rms_range[CONF_MAX_DB]))
    band_range = config[CONF_BAND_RANGE]
    cg.add(var.set_band_range(band_range[CONF_MIN_DB], band_range[CONF_MAX_DB]))
    cg.add(var.set_buffer_samples(math.ceil(buffer_ms * plan.rate / 1000)))


CONFIG_SCHEMA = cv.All(
    ANALYZER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MicrophoneAudioAnalyzer),
            cv.Optional(
                CONF_MICROPHONE, default={}
            ): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                max_channels=1,
            ),
            cv.Required(CONF_PASSIVE): cv.boolean,
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
)


def _microphone_sample_rate(
    config: ConfigType, full_config: fv.FinalValidateConfig
) -> int:
    mic_config = fv.get_declaring_config(
        config[CONF_MICROPHONE][CONF_MICROPHONE], full_config
    )
    low = mic_config.get(audio.CONF_MIN_SAMPLE_RATE)
    high = mic_config.get(audio.CONF_MAX_SAMPLE_RATE)
    if low is None or low != high:
        raise cv.Invalid(
            "The microphone must have one fixed sample rate", path=[CONF_MICROPHONE]
        )
    return low


def _final_validate(config: ConfigType) -> ConfigType:
    microphone.final_validate_microphone_source_schema(DOMAIN)(config[CONF_MICROPHONE])
    analysis_plan(config, _microphone_sample_rate(config, fv.full_config.get()))
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE], passive=config[CONF_PASSIVE]
    )
    sample_rate = _microphone_sample_rate(config, CORE.config)
    var = cg.new_Pvariable(config[CONF_ID], mic_source, sample_rate)
    await setup_analyzer(var, config, sample_rate, MICROPHONE_BUFFER_MS)


AUDIO_ANALYZER_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(AudioAnalyzer)}
)


automation.register_apply_action(
    "audio_analyzer.start",
    AUDIO_ANALYZER_ACTION_SCHEMA,
    automation.ApplyCall("start_analysis()"),
)
automation.register_apply_action(
    "audio_analyzer.stop",
    AUDIO_ANALYZER_ACTION_SCHEMA,
    automation.ApplyCall("stop_analysis()"),
)
