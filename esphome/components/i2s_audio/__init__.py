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
from esphome.cpp_generator import MockObjClass
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

CONF_LEFT = "left"
CONF_RIGHT = "right"
CONF_STEREO = "stereo"

i2s_audio_ns = cg.esphome_ns.namespace("i2s_audio")
I2SAudioComponent = i2s_audio_ns.class_("I2SAudioComponent", cg.Component)
I2SAudioBase = i2s_audio_ns.class_(
    "I2SAudioBase", cg.Parented.template(I2SAudioComponent)
)
I2SAudioIn = i2s_audio_ns.class_("I2SAudioIn", I2SAudioBase)
I2SAudioOut = i2s_audio_ns.class_("I2SAudioOut", I2SAudioBase)

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
I2S_CHANNELS = {
    CONF_LEFT: i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_LEFT,
    CONF_RIGHT: i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_RIGHT,
    CONF_STEREO: i2s_channel_fmt_t.I2S_CHANNEL_FMT_RIGHT_LEFT,
}

i2s_bits_per_sample_t = cg.global_ns.enum("i2s_bits_per_sample_t")
I2S_BITS_PER_SAMPLE = {
    8: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_8BIT,
    16: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_16BIT,
    32: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_32BIT,
}

INTERNAL_ADC_VARIANTS = [VARIANT_ESP32]
PDM_VARIANTS = [VARIANT_ESP32, VARIANT_ESP32S3]

_validate_bits = cv.float_with_unit("bits", "bit")


def i2s_audio_component_schema(
    class_: MockObjClass,
    default_sample_rate: int,
    default_channel: str,
    default_bits_per_sample: str,
):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
            cv.Optional(CONF_CHANNEL, default=default_channel): cv.enum(I2S_CHANNELS),
            cv.Optional(CONF_SAMPLE_RATE, default=default_sample_rate): cv.int_range(
                min=1
            ),
            cv.Optional(CONF_BITS_PER_SAMPLE, default=default_bits_per_sample): cv.All(
                _validate_bits, cv.enum(I2S_BITS_PER_SAMPLE)
            ),
            cv.Optional(CONF_I2S_MODE, default=CONF_PRIMARY): cv.enum(
                I2S_MODE_OPTIONS, lower=True
            ),
        }
    )


async def register_i2s_audio_component(var, config):
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
