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
)
from esphome.core import CORE, ID
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

CONF_OUTPUT_SPEAKERS = "output_speakers"
CONF_SOURCE_SPEAKERS = "source_speakers"

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
            # How much analysis history to keep, not audio waiting to play. Must cover how far the audio written to
            # this speaker runs ahead of what is heard (buffers of the output speaker and of anything after it).
            # Defaults to the output chain's buffers plus a margin.
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
    cv.only_on_esp32,
    _validate_bands,
    _set_stream_limits,
)


# Added to the output chain's buffers for the default buffer_duration: transfer and DMA buffers, and the main loop
OUTPUT_LATENCY_MARGIN_MS = 250


def _mixer_output_speaker(
    speaker_id: ID, full_config: fv.FinalValidateConfig
) -> ID | None:
    """The output speaker of the mixer a source speaker belongs to, which the mixer only copies into the source
    during its own final validation."""
    path = full_config.get_path_for_id(speaker_id)[:-1]
    if len(path) < 2 or path[-2] != CONF_SOURCE_SPEAKERS:
        return None
    return full_config.get_config_for_path([*path[:-2], CONF_OUTPUT_SPEAKER])


def output_latency_ms(
    config: ConfigType, full_config: fv.FinalValidateConfig
) -> tuple[int, bool]:
    """Sum of the buffer_duration of the speakers the audio passes through after this one, and whether the walk
    reached the speaker that plays the audio. It doesn't when a speaker it can't follow is in the way: a router (one
    of several outputs), or a speaker without a buffer_duration."""
    total = 0
    seen: set[str] = set()
    speaker_id = config[CONF_OUTPUT_SPEAKER]
    while str(speaker_id) not in seen:
        seen.add(str(speaker_id))
        block = fv.get_declaring_config(speaker_id, full_config)
        duration = block.get(CONF_BUFFER_DURATION)
        # Another analyzer's buffer_duration is its analysis buffer, not audio waiting to be played
        if block.get(CONF_PLATFORM) != DOMAIN and duration is not None:
            total += duration.total_milliseconds
        next_id = block.get(CONF_OUTPUT_SPEAKER) or _mixer_output_speaker(
            speaker_id, full_config
        )
        if next_id is None:
            # A speaker that plays the audio buffers it and names no other speaker
            return total, duration is not None and CONF_OUTPUT_SPEAKERS not in block
        speaker_id = next_id
    return total, False


def _final_validate(config: ConfigType) -> ConfigType:
    audio.final_validate_audio_schema(
        "audio_analyzer speaker",
        audio_device=CONF_OUTPUT_SPEAKER,
        bits_per_sample=config[CONF_BITS_PER_SAMPLE],
        channels=config[CONF_NUM_CHANNELS],
        sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    if (buffer := config.get(CONF_BUFFER_DURATION)) is not None:
        latency, complete = output_latency_ms(config, fv.full_config.get())
        if complete and buffer.total_milliseconds < latency:
            # The analysis would never catch up with what is heard
            raise cv.Invalid(
                f"{CONF_BUFFER_DURATION} must cover the {latency} ms of audio the output speakers buffer: set at least "
                f"{latency + OUTPUT_LATENCY_MARGIN_MS} ms, or leave it out",
                path=[CONF_BUFFER_DURATION],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    output_speaker = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    var = cg.new_Pvariable(config[CONF_ID], output_speaker, config[CONF_SAMPLE_RATE])
    if (buffer := config.get(CONF_BUFFER_DURATION)) is not None:
        buffer_ms = buffer.total_milliseconds
    else:
        latency, _ = output_latency_ms(config, CORE.config)
        buffer_ms = min(5000, latency + OUTPUT_LATENCY_MARGIN_MS)
    await setup_analyzer(var, config, config[CONF_SAMPLE_RATE], buffer_ms)
    await speaker.register_speaker(var, config)
