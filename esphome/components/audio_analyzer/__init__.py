"""Live RMS level and frequency spectrum of an audio stream.

One analysis engine with two inputs:
- ``audio_analyzer:`` analyzes a microphone, hooked up like ``sound_level``.
- ``speaker: - platform: audio_analyzer`` is a pass-through speaker that analyzes the audio it plays.

Values are read in lambdas through the getters; ``sensor: - platform: audio_analyzer`` publishes them at a low rate.
"""

import math

from esphome import automation
import esphome.codegen as cg
from esphome.components import microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MICROPHONE, CONF_SAMPLE_RATE, PLATFORM_ESP32
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["audio"]
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

# Must match analysis.h
NUM_BANDS = 12
FFT_SIZE = 512
MAX_ANALYSIS_RATE_HZ = 16000
MAX_BAND_FRACTION = 0.3
MIN_BAND_BINS = 2

audio_analyzer_ns = cg.esphome_ns.namespace("audio_analyzer")
AudioAnalyzer = audio_analyzer_ns.class_("AudioAnalyzer", cg.Component)
MicrophoneAudioAnalyzer = audio_analyzer_ns.class_(
    "MicrophoneAudioAnalyzer", AudioAnalyzer
)


def analysis_rate(sample_rate: int) -> float:
    """Rate a stream is analyzed at: decimated by an integer factor to at most 16 kHz (``decimation_for``)."""
    return sample_rate / max(1, math.ceil(sample_rate / MAX_ANALYSIS_RATE_HZ))


def _validate_db_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_DB] >= config[CONF_MAX_DB]:
        raise cv.Invalid(f"{CONF_MIN_DB} must be below {CONF_MAX_DB}")
    return config


def _db_range_schema(min_default: float, max_default: float) -> cv.All:
    return cv.All(
        cv.Schema(
            {
                cv.Optional(CONF_MIN_DB, default=min_default): cv.float_range(
                    min=-100, max=0
                ),
                cv.Optional(CONF_MAX_DB, default=max_default): cv.float_range(
                    min=-100, max=0
                ),
            }
        ),
        _validate_db_range,
    )


def _round_bin(frequency: float, bin_width: float) -> int:
    """FFT bin of a frequency, rounded like ``lroundf`` in ``SpectrumAnalysis::configure``."""
    return math.floor(frequency / bin_width + 0.5)


def validate_frequency_range(config: ConfigType) -> ConfigType:
    min_frequency = config[CONF_MIN_FREQUENCY]
    max_frequency = config[CONF_MAX_FREQUENCY]
    if min_frequency >= max_frequency:
        raise cv.Invalid(f"{CONF_MIN_FREQUENCY} must be below {CONF_MAX_FREQUENCY}")
    # Every band gets at least MIN_BAND_BINS FFT bins; the bins are widest at the highest analysis rate
    bin_width = MAX_ANALYSIS_RATE_HZ / FFT_SIZE
    lower_bin = max(1, _round_bin(min_frequency, bin_width))
    if _round_bin(max_frequency, bin_width) - lower_bin < NUM_BANDS * MIN_BAND_BINS:
        needed = (lower_bin + NUM_BANDS * MIN_BAND_BINS - 0.5) * bin_width
        raise cv.Invalid(
            f"The range is too narrow for {NUM_BANDS} bands of at least {MIN_BAND_BINS * bin_width} Hz "
            f"({MIN_BAND_BINS} analysis bins): raise {CONF_MAX_FREQUENCY} to at least {math.ceil(needed)} Hz "
            f"or lower {CONF_MIN_FREQUENCY}",
            path=[CONF_MAX_FREQUENCY],
        )
    return config


def validate_max_frequency_for_rate(config: ConfigType, sample_rate: int) -> None:
    """Rejects bands above what a stream rate that is known at config time allows."""
    limit = MAX_BAND_FRACTION * analysis_rate(sample_rate)
    if config[CONF_MAX_FREQUENCY] > limit:
        raise cv.Invalid(
            f"{CONF_MAX_FREQUENCY} must be at most {math.floor(limit)} Hz for a {sample_rate} Hz stream",
            path=[CONF_MAX_FREQUENCY],
        )


# Options shared by the microphone analyzer and the speaker platform.
ANALYZER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_FREQUENCY, default="100Hz"): cv.All(
            cv.frequency, cv.Range(min=20, max=4000)
        ),
        cv.Optional(CONF_MAX_FREQUENCY, default="4000Hz"): cv.All(
            cv.frequency,
            cv.Range(min=100, max=MAX_BAND_FRACTION * MAX_ANALYSIS_RATE_HZ),
        ),
        cv.Optional(CONF_RMS_RANGE, default={}): _db_range_schema(-70, -20),
        cv.Optional(CONF_BAND_RANGE, default={}): _db_range_schema(-80, -30),
        cv.Optional(CONF_ATTACK, default="30ms"): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_RELEASE, default="250ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_analyzer(var: cg.MockObj, config: ConfigType) -> None:
    """Registers the component and applies the shared analysis options."""
    await cg.register_component(var, config)
    cg.add(
        var.set_frequency_range(config[CONF_MIN_FREQUENCY], config[CONF_MAX_FREQUENCY])
    )
    rms_range = config[CONF_RMS_RANGE]
    cg.add(var.set_rms_range(rms_range[CONF_MIN_DB], rms_range[CONF_MAX_DB]))
    band_range = config[CONF_BAND_RANGE]
    cg.add(var.set_band_range(band_range[CONF_MIN_DB], band_range[CONF_MAX_DB]))
    cg.add(var.set_attack(config[CONF_ATTACK]))
    cg.add(var.set_release(config[CONF_RELEASE]))


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
    validate_frequency_range,
    cv.only_on([PLATFORM_ESP32]),
)


def _final_validate(config: ConfigType) -> ConfigType:
    microphone.final_validate_microphone_source_schema(DOMAIN)(config[CONF_MICROPHONE])
    mic_config = fv.get_declaring_config(
        config[CONF_MICROPHONE][CONF_MICROPHONE], fv.full_config.get()
    )
    if isinstance(sample_rate := mic_config.get(CONF_SAMPLE_RATE), int):
        validate_max_frequency_for_rate(config, sample_rate)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    mic_source = await microphone.microphone_source_to_code(
        config[CONF_MICROPHONE], passive=config[CONF_PASSIVE]
    )
    var = cg.new_Pvariable(config[CONF_ID], mic_source)
    await setup_analyzer(var, config)


AUDIO_ANALYZER_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(MicrophoneAudioAnalyzer),
    }
)


automation.register_apply_action(
    "audio_analyzer.start",
    AUDIO_ANALYZER_ACTION_SCHEMA,
    automation.ApplyCall("start()"),
)
automation.register_apply_action(
    "audio_analyzer.stop",
    AUDIO_ANALYZER_ACTION_SCHEMA,
    automation.ApplyCall("stop()"),
)
