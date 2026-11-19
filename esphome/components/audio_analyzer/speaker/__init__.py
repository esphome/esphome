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
    CONF_PLATFORM,
    CONF_SAMPLE_RATE,
    PLATFORM_ESP32,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    ANALYZER_SCHEMA,
    DOMAIN,
    AudioAnalyzer,
    analysis_plan,
    audio_analyzer_ns,
    setup_analyzer,
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


def _validate_bands(config: ConfigType) -> ConfigType:
    analysis_plan(config, config[CONF_SAMPLE_RATE])
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
            # output speaker and of anything after it). Defaults to the output chain's buffers plus a margin.
            cv.Optional(CONF_BUFFER_DURATION): cv.All(
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
    _validate_bands,
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
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# Added to the output chain's buffers for the default buffer_duration: transfer and DMA buffers, and the main loop
OUTPUT_LATENCY_MARGIN_MS = 250


def output_latency_ms(config: ConfigType, full_config: fv.FinalValidateConfig) -> int:
    """Sum of the buffer_duration of the speakers the audio passes through after this one."""
    total = 0
    seen: set[str] = set()
    speaker_id = config[CONF_OUTPUT_SPEAKER]
    while speaker_id is not None and str(speaker_id) not in seen:
        seen.add(str(speaker_id))
        block = fv.get_declaring_config(speaker_id, full_config)
        # Another analyzer's buffer_duration is its analysis buffer, not audio waiting to be played
        if block.get(CONF_PLATFORM) != DOMAIN and (
            duration := block.get(CONF_BUFFER_DURATION)
        ):
            total += duration.total_milliseconds
        # Mixer source speakers get their mixer's output_speaker during validation
        speaker_id = block.get(CONF_OUTPUT_SPEAKER)
    return total


async def to_code(config: ConfigType) -> None:
    output_speaker = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    var = cg.new_Pvariable(config[CONF_ID], output_speaker, config[CONF_SAMPLE_RATE])
    if (buffer := config.get(CONF_BUFFER_DURATION)) is not None:
        buffer_ms = buffer.total_milliseconds
    else:
        buffer_ms = min(
            5000, output_latency_ms(config, CORE.config) + OUTPUT_LATENCY_MARGIN_MS
        )
    await setup_analyzer(var, config, config[CONF_SAMPLE_RATE], buffer_ms)
    await speaker.register_speaker(var, config)
