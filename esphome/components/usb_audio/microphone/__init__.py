import esphome.codegen as cg
from esphome.components import audio, microphone
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
)

from .. import CONF_USB_AUDIO_ID, USBAudioComponent, usb_audio_ns

USBAudioMicrophone = usb_audio_ns.class_(
    "USBAudioMicrophone", microphone.Microphone, cg.Component
)

BITS_PER_SAMPLE_OPTIONS = [8, 16, 24, 32]


def _bits_validator():
    return cv.All(
        cv.float_with_unit("bits", "bit"), cv.one_of(*BITS_PER_SAMPLE_OPTIONS)
    )


def _set_stream_limits(config):
    bits_per_sample = int(config[CONF_BITS_PER_SAMPLE])

    audio.set_stream_limits(
        min_bits_per_sample=bits_per_sample,
        max_bits_per_sample=bits_per_sample,
        min_channels=config[CONF_NUM_CHANNELS],
        max_channels=config[CONF_NUM_CHANNELS],
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(USBAudioMicrophone),
                cv.GenerateID(CONF_USB_AUDIO_ID): cv.use_id(USBAudioComponent),
                cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(
                    min=8000, max=96000
                ),
                cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): _bits_validator(),
                cv.Optional(CONF_NUM_CHANNELS, default=1): cv.int_range(min=1, max=1),
            }
        ).extend(cv.COMPONENT_SCHEMA)
    ),
    _set_stream_limits,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    parent = await cg.get_variable(config[CONF_USB_AUDIO_ID])
    await cg.register_parented(var, config[CONF_USB_AUDIO_ID])

    sample_rate = int(config[CONF_SAMPLE_RATE])
    bits_per_sample = int(config[CONF_BITS_PER_SAMPLE])
    channels = int(config[CONF_NUM_CHANNELS])

    cg.add(var.set_sample_rate(sample_rate))
    cg.add(var.set_bits_per_sample(bits_per_sample))
    cg.add(var.set_channels(channels))

    cg.add(parent.set_microphone(var))
    cg.add(parent.set_microphone_params(channels, bits_per_sample, sample_rate))
