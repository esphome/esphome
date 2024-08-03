from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_SAMPLE_RATE
import esphome.final_validate as fv

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_DIN_PIN = "i2s_din_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"

CONF_I2S_AUDIO = "i2s_audio"
CONF_I2S_AUDIO_ID = "i2s_audio_id"

CONF_BITS_PER_SAMPLE = "bits_per_sample"
CONF_I2S_MODE = "i2s_mode"
CONF_PRIMARY = "primary"
CONF_SECONDARY = "secondary"

i2s_audio_ns = cg.esphome_ns.namespace("i2s_audio")
I2SAudioComponent = i2s_audio_ns.class_("I2SAudioComponent", cg.Component)
I2SAudioIn = i2s_audio_ns.class_("I2SAudioIn", cg.Parented.template(I2SAudioComponent))
I2SAudioOut = i2s_audio_ns.class_(
    "I2SAudioOut", cg.Parented.template(I2SAudioComponent)
)

i2s_mode_t = cg.global_ns.enum("i2s_mode_t")
I2S_MODE_OPTIONS = {
    CONF_PRIMARY: i2s_mode_t.I2S_MODE_MASTER,  # NOLINT
    CONF_SECONDARY: i2s_mode_t.I2S_MODE_SLAVE,  # NOLINT
}

# https://github.com/espressif/esp-idf/blob/master/components/soc/{variant}/include/soc/soc_caps.h
I2S_PORTS = {
    VARIANT_ESP32: 2,
    VARIANT_ESP32S2: 1,
    VARIANT_ESP32S3: 2,
    VARIANT_ESP32C3: 1,
}

i2s_channel_fmt_t = cg.global_ns.enum("i2s_channel_fmt_t")
CHANNELS = {
    "left": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_LEFT,
    "right": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_RIGHT,
    "all_left": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ALL_LEFT,
    "all_right": i2s_channel_fmt_t.I2S_CHANNEL_FMT_ALL_RIGHT,
    "stereo": i2s_channel_fmt_t.I2S_CHANNEL_FMT_RIGHT_LEFT,
}

i2s_bits_per_sample_t = cg.global_ns.enum("i2s_bits_per_sample_t")
BITS_PER_SAMPLE = {
    8: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_8BIT,
    16: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_16BIT,
    32: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_32BIT,
}

INTERNAL_ADC_VARIANTS = [VARIANT_ESP32]
PDM_VARIANTS = [VARIANT_ESP32, VARIANT_ESP32S3]

_validate_bits = cv.float_with_unit("bits", "bit")


def I2SAudioSchema(component_id, sample_rate, channels, bits_per_sample):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(component_id),
            cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
            cv.Optional(CONF_CHANNEL, default=channels): cv.enum(CHANNELS),
            cv.Optional(CONF_SAMPLE_RATE, default=sample_rate): cv.int_range(min=1),
            cv.Optional(CONF_BITS_PER_SAMPLE, default=bits_per_sample): cv.All(
                _validate_bits, cv.enum(BITS_PER_SAMPLE)
            ),
            cv.Optional(CONF_I2S_MODE, default=CONF_PRIMARY): cv.enum(
                I2S_MODE_OPTIONS, lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)


async def register_i2saudio(var, config):
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])

    cg.add(var.set_i2s_mode(config[CONF_I2S_MODE]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(I2SAudioComponent),
        cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_MCLK_PIN): pins.internal_gpio_output_pin_number,
    }
)


def _final_validate(_):
    i2s_audio_configs = fv.full_config.get()[CONF_I2S_AUDIO]
    variant = get_esp32_variant()
    if variant not in I2S_PORTS:
        raise cv.Invalid(f"Unsupported variant {variant}")
    if len(i2s_audio_configs) > I2S_PORTS[variant]:
        raise cv.Invalid(
            f"Only {I2S_PORTS[variant]} I2S audio ports are supported on {variant}"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))
    if CONF_I2S_BCLK_PIN in config:
        cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
    if CONF_I2S_MCLK_PIN in config:
        cg.add(var.set_mclk_pin(config[CONF_I2S_MCLK_PIN]))
