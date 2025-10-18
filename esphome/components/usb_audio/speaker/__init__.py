from esphome.components import audio, speaker
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
)

from .. import CONF_USB_AUDIO_ID, USBAudioComponent, usb_audio_ns

USBAudioSpeaker = usb_audio_ns.class_("USBAudioSpeaker", speaker.Speaker, cg.Component)

CONF_WRITE_TIMEOUT = "write_timeout"
BITS_PER_SAMPLE_OPTIONS = [8, 16, 24, 32]


def _bits_validator():
    return cv.All(cv.float_with_unit("bits", "bit"), cv.one_of(*BITS_PER_SAMPLE_OPTIONS))


def _set_stream_limits(config):
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
    speaker.SPEAKER_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(USBAudioSpeaker),
                cv.GenerateID(CONF_USB_AUDIO_ID): cv.use_id(USBAudioComponent),
                cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.int_range(min=8000, max=96000),
                cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): _bits_validator(),
                cv.Optional(CONF_NUM_CHANNELS, default=2): cv.int_range(min=1, max=2),
                cv.Optional(CONF_WRITE_TIMEOUT, default="20ms"): cv.positive_time_period_milliseconds,
            }
        ).extend(cv.COMPONENT_SCHEMA)
    ),
    _set_stream_limits,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)

    parent = await cg.get_variable(config[CONF_USB_AUDIO_ID])
    await cg.register_parented(var, config[CONF_USB_AUDIO_ID])

    sample_rate = int(config[CONF_SAMPLE_RATE])
    bits_per_sample = int(config[CONF_BITS_PER_SAMPLE])
    channels = int(config[CONF_NUM_CHANNELS])
    write_timeout = config[CONF_WRITE_TIMEOUT].total_milliseconds

    cg.add(var.set_sample_rate(sample_rate))
    cg.add(var.set_bits_per_sample(bits_per_sample))
    cg.add(var.set_channels(channels))
    cg.add(var.set_write_timeout(write_timeout))

    cg.add(parent.set_speaker(var))
    cg.add(parent.set_speaker_params(channels, bits_per_sample, sample_rate))

