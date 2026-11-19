import esphome.codegen as cg
from esphome.components import audio, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUDIO_DAC,
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
    PLATFORM_ESP32,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    ANALYZER_SCHEMA,
    AudioAnalyzer,
    audio_analyzer_ns,
    setup_analyzer,
    validate_frequency_range,
    validate_max_frequency_for_rate,
)

AUTO_LOAD = ["audio", "audio_analyzer"]

AudioAnalyzerSpeaker = audio_analyzer_ns.class_(
    "AudioAnalyzerSpeaker", AudioAnalyzer, speaker.Speaker
)

# The audio passes through unchanged, so this speaker takes the output speaker's format and stream limits
_INHERITED_KEYS = (
    CONF_BITS_PER_SAMPLE,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    audio.CONF_MIN_BITS_PER_SAMPLE,
    audio.CONF_MAX_BITS_PER_SAMPLE,
    audio.CONF_MIN_CHANNELS,
    audio.CONF_MAX_CHANNELS,
    audio.CONF_MIN_SAMPLE_RATE,
    audio.CONF_MAX_SAMPLE_RATE,
)

_PASS_THROUGH = "The audio passes through unchanged: set {} on the output speaker"

CONFIG_SCHEMA = cv.All(
    ANALYZER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AudioAnalyzerSpeaker),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            # Must cover how far the audio written to this speaker runs ahead of what is heard (buffers of the
            # output speaker and of anything after it). Holds 16-bit samples at 16 kHz: 32 KB per second.
            cv.Optional(CONF_BUFFER_DURATION, default="1000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=100),
                    max=cv.TimePeriod(milliseconds=5000),
                ),
            ),
            **{
                cv.Optional(key): cv.invalid(_PASS_THROUGH.format(key))
                for key in (
                    CONF_AUDIO_DAC,
                    CONF_BITS_PER_SAMPLE,
                    CONF_NUM_CHANNELS,
                    CONF_SAMPLE_RATE,
                )
            },
        }
    ),
    validate_frequency_range,
    cv.only_on([PLATFORM_ESP32]),
)


def _final_validate(config: ConfigType) -> ConfigType:
    # Copy the output's format and limits so components that write to this speaker (media players, mixers) validate
    # against what the output accepts. Like any inherited setting, they are only seen by components validated after
    # this one.
    full_config = fv.full_config.get()
    output_config = fv.get_declaring_config(config[CONF_OUTPUT_SPEAKER], full_config)
    this_config = fv.get_declaring_config(config[CONF_ID], full_config)
    for key in _INHERITED_KEYS:
        # A resampler's "passthrough" bits_per_sample is decided at run time, so there is nothing to inherit
        if isinstance(value := output_config.get(key), int):
            this_config[key] = value
            config[key] = value
    if (sample_rate := config.get(CONF_SAMPLE_RATE)) is not None:
        validate_max_frequency_for_rate(config, sample_rate)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    output_speaker = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    var = cg.new_Pvariable(config[CONF_ID], output_speaker)
    await setup_analyzer(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
