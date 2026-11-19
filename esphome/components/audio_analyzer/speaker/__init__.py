import esphome.codegen as cg
from esphome.components import speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_DURATION,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_OUTPUT_SPEAKER,
    CONF_SAMPLE_RATE,
    PLATFORM_ESP32,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

from .. import (
    ANALYZER_SCHEMA,
    AudioAnalyzer,
    audio_analyzer_ns,
    setup_analyzer,
    validate_frequency_range,
)

AUTO_LOAD = ["audio"]

AudioAnalyzerSpeaker = audio_analyzer_ns.class_(
    "AudioAnalyzerSpeaker", AudioAnalyzer, speaker.Speaker
)

CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(ANALYZER_SCHEMA).extend(
        {
            cv.GenerateID(): cv.declare_id(AudioAnalyzerSpeaker),
            cv.Required(CONF_OUTPUT_SPEAKER): cv.use_id(speaker.Speaker),
            # Must cover how far the audio written to this speaker runs ahead of what is heard (buffers of the
            # output speaker and of anything after it)
            cv.Optional(
                CONF_BUFFER_DURATION, default="1000ms"
            ): cv.positive_time_period_milliseconds,
        }
    ),
    validate_frequency_range,
    cv.only_on([PLATFORM_ESP32]),
)

# The audio passes through unchanged, so this speaker accepts what the output speaker accepts.
FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_BITS_PER_SAMPLE, CONF_OUTPUT_SPEAKER),
    inherit_property_from(CONF_NUM_CHANNELS, CONF_OUTPUT_SPEAKER),
    inherit_property_from(CONF_SAMPLE_RATE, CONF_OUTPUT_SPEAKER),
)


async def to_code(config: ConfigType) -> None:
    output_speaker = await cg.get_variable(config[CONF_OUTPUT_SPEAKER])
    var = cg.new_Pvariable(config[CONF_ID], output_speaker)
    await setup_analyzer(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
