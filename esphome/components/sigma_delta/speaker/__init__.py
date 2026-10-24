"""Sigma delta speaker platform.

Drives a GPIO with the ESP32 sigma-delta modulator; an RC low-pass filter on the pin recovers the audio.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_SAMPLE_RATE
from esphome.types import ConfigType

DEPENDENCIES = ["esp32"]

sigma_delta_ns = cg.esphome_ns.namespace("sigma_delta")
SigmaDeltaSpeaker = sigma_delta_ns.class_(
    "SigmaDeltaSpeaker", cg.Component, speaker.Speaker
)


def _set_stream_limits(config: ConfigType) -> ConfigType:
    # Any bit depth and channel count is mixed down in software; the output runs at the configured rate
    audio.set_stream_limits(
        min_bits_per_sample=8,
        max_bits_per_sample=32,
        min_channels=1,
        max_channels=2,
        min_sample_rate=config[CONF_SAMPLE_RATE],
        max_sample_rate=config[CONF_SAMPLE_RATE],
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SigmaDeltaSpeaker),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_SAMPLE_RATE, default=44100): cv.int_range(8000, 48000),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        unsupported=[esp32.VARIANT_ESP32C2, esp32.VARIANT_ESP32H2],
        msg_prefix="Sigma delta speaker",
    ),
    _set_stream_limits,
)


async def to_code(config: ConfigType) -> None:
    esp32.include_builtin_idf_component("esp_driver_sdm")
    esp32.include_builtin_idf_component("esp_driver_gptimer")

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)
