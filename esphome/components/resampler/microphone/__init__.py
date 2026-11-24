import esphome.codegen as cg
from esphome.components import audio, microphone
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_CHANNELS,
    CONF_FILTERS,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_SAMPLE_RATE,
    PLATFORM_ESP32,
)
from esphome.types import ConfigType

from .. import CONF_TAPS, resampler_ns, validate_taps

AUTO_LOAD = ["audio"]
DEPENDENCIES = ["microphone"]

ResamplerMicrophone = resampler_ns.class_(
    "ResamplerMicrophone", cg.Component, microphone.Microphone
)


def _set_stream_limits(config: ConfigType) -> ConfigType:
    # Only the sample rate changes; the bits and channels are those selected from the source microphone
    source = config[CONF_MICROPHONE]
    audio.set_stream_limits(
        min_bits_per_sample=source[CONF_BITS_PER_SAMPLE],
        max_bits_per_sample=source[CONF_BITS_PER_SAMPLE],
        min_channels=len(source[CONF_CHANNELS]),
        max_channels=len(source[CONF_CHANNELS]),
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ResamplerMicrophone),
            cv.Required(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=32,
                min_channels=1,
                max_channels=2,
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(8000, 48000),
            cv.Optional(CONF_FILTERS, default=16): cv.int_range(min=2, max=1024),
            cv.Optional(CONF_TAPS, default=16): validate_taps,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32]),
    _set_stream_limits,
)


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(
            CONF_MICROPHONE
        ): microphone.final_validate_microphone_source_schema("resampler"),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config: ConfigType) -> None:
    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    var = cg.new_Pvariable(config[CONF_ID], mic_source)
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    cg.add(var.set_target_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_filters(config[CONF_FILTERS]))
    cg.add(var.set_taps(config[CONF_TAPS]))
