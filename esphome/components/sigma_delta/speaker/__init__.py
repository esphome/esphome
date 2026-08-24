"""Sigma delta speaker platform for ESPHome.

Uses the ESP32's sigma-delta modulation peripheral to generate a 1-bit
PDM bitstream on a GPIO.  An RC low-pass filter reconstructs the analog
audio signal.

Wire: GPIO --[1kΩ]--+--> speaker/headphone
                   [10nF] -> GND  (cutoff ≈16 kHz)
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_PIN,
    CONF_SAMPLE_RATE,
)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32"]

sigma_delta_ns = cg.esphome_ns.namespace("sigma_delta")
SigmaDeltaSpeaker = sigma_delta_ns.class_(
    "SigmaDeltaSpeaker", cg.Component, speaker.Speaker
)

CONF_OVERSAMPLE_RATE = "oversample_rate"

CONFIG_SCHEMA = speaker.SPEAKER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SigmaDeltaSpeaker),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_SAMPLE_RATE, default=44100): cv.int_range(min=8000, max=48000),
        cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.one_of(
            8, 16, 24, 32, int=True
        ),
        cv.Optional(CONF_NUM_CHANNELS, default=2): cv.int_range(min=1, max=2),
        cv.Optional(CONF_OVERSAMPLE_RATE, default=1_000_000): cv.int_range(
            min=100000, max=5_000_000
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_variant(value):
    variant = esp32.get_esp32_variant()
    if variant in (esp32.VARIANT_ESP32C2, esp32.VARIANT_ESP32H2):
        raise cv.Invalid(f"Sigma delta speaker is not supported on {variant}")
    return value


def _set_stream_limits(config):
    # Inform media_pipeline what formats we accept
    audio.set_stream_limits(
        min_bits_per_sample=8,
        max_bits_per_sample=32,
        min_channels=1,
        max_channels=2,
        min_sample_rate=8000,
        max_sample_rate=48000,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_variant, _set_stream_limits)


async def to_code(config):
    # Re-enable the ESP-IDF sigma-delta driver (excluded by default)
    esp32.include_builtin_idf_component("esp_driver_sdm")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_oversample_rate(config[CONF_OVERSAMPLE_RATE]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_num_channels(config[CONF_NUM_CHANNELS]))
