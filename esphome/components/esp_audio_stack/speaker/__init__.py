"""ESP Audio Stack Speaker Platform - Wraps full-duplex bus as standard ESPHome speaker"""

import esphome.codegen as cg
from esphome.components import audio, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_DURATION,
    CONF_ID,
    CONF_NEVER,
    CONF_NUM_CHANNELS,
    CONF_TIMEOUT,
)

from .. import CONF_ESP_AUDIO_STACK_ID, ESPAudioStack, esp_audio_stack_ns

DEPENDENCIES = ["esp_audio_stack"]
CODEOWNERS = ["@n-IA-hane"]

ESPAudioStackSpeaker = esp_audio_stack_ns.class_(
    "ESPAudioStackSpeaker",
    speaker.Speaker,
    cg.Component,
    cg.Parented.template(ESPAudioStack),
)


def _set_audio_properties(config):
    """Set default audio properties for mixer/resampler compatibility.

    Note: sample_rate is NOT defaulted here because the speaker operates at bus
    rate (e.g. 48kHz for ES8311), not a fixed 16kHz. The C++ setup() sets
    audio_stream_info_ from parent->get_sample_rate() at runtime.
    """
    if CONF_NUM_CHANNELS not in config:
        config[CONF_NUM_CHANNELS] = 1
    return config


def _set_stream_limits(config):
    # Speaker accepts audio at bus rate (e.g. 48kHz). ResamplerSpeaker handles upsampling
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=1,
        max_channels=2,
        min_sample_rate=8000,
        max_sample_rate=48000,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ESPAudioStackSpeaker),
            cv.GenerateID(CONF_ESP_AUDIO_STACK_ID): cv.use_id(ESPAudioStack),
            cv.Optional(
                CONF_BUFFER_DURATION, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIMEOUT, default=CONF_NEVER): cv.Any(
                cv.positive_time_period_milliseconds,
                cv.one_of(CONF_NEVER, lower=True),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _set_audio_properties,
    _set_stream_limits,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_ESP_AUDIO_STACK_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_speaker_buffer_duration(config[CONF_BUFFER_DURATION]))
    if config[CONF_TIMEOUT] != CONF_NEVER:
        cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    await speaker.register_speaker(var, config)
