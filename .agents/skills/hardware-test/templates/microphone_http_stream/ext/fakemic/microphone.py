"""Test-only microphone that emits a 16-bit mono counter pattern, so a reader can check every sample."""

import esphome.codegen as cg
from esphome.components import audio, microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SAMPLE_RATE
from esphome.types import ConfigType

from . import fakemic_ns

FakeMicrophone = fakemic_ns.class_(
    "FakeMicrophone", microphone.Microphone, cg.Component
)


def _limits(config: ConfigType) -> ConfigType:
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=1,
        max_channels=1,
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(FakeMicrophone),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(8000, 48000),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _limits,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_SAMPLE_RATE])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
