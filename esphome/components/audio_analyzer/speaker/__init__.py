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


def _set_stream_limits(config: ConfigType) -> ConfigType:
    # Lock the stream limits to the declared format at CONFIG_SCHEMA time, like router: components that write to this
    # speaker (media players, mixers) read them during their FINAL_VALIDATE, which can run before ours
    audio.set_stream_limits(
        min_bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        max_bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        min_channels=config[CONF_NUM_CHANNELS],
        max_channels=config[CONF_NUM_CHANNELS],
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    ANALYZER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AudioAnalyzerSpeaker),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            # The format of the audio written to this speaker. It passes through unchanged, so the output speaker
            # must accept it. Required rather than inherited from the output so that validation doesn't depend on
            # the order of the configuration.
            cv.Required(CONF_BITS_PER_SAMPLE): cv.int_range(8, 32),
            cv.Required(CONF_NUM_CHANNELS): cv.int_range(1, 2),
            cv.Required(CONF_SAMPLE_RATE): cv.int_range(8000, 48000),
            # Must cover how far the audio written to this speaker runs ahead of what is heard (buffers of the
            # output speaker and of anything after it). Holds 16-bit samples at 16 kHz: 32 KB per second.
            cv.Optional(CONF_BUFFER_DURATION, default="1000ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=100),
                    max=cv.TimePeriod(milliseconds=5000),
                ),
            ),
            cv.Optional(CONF_AUDIO_DAC): cv.invalid(
                "The audio passes through unchanged: set audio_dac on the output speaker"
            ),
        }
    ),
    validate_frequency_range,
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)


def _final_validate(config: ConfigType) -> ConfigType:
    audio.final_validate_audio_schema(
        "audio_analyzer speaker",
        audio_device=CONF_OUTPUT_SPEAKER,
        bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        channels=config[CONF_NUM_CHANNELS],
        sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    validate_max_frequency_for_rate(config, config[CONF_SAMPLE_RATE])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    output_speaker = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    var = cg.new_Pvariable(config[CONF_ID], output_speaker)
    await setup_analyzer(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
