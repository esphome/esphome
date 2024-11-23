from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, speaker
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_MODE, CONF_TIMEOUT

from .. import (
    CONF_I2S_DOUT_PIN,
    CONF_LEFT,
    CONF_MONO,
    CONF_RIGHT,
    CONF_STEREO,
    I2SAudioOut,
    i2s_audio_component_schema,
    i2s_audio_ns,
    register_i2s_audio_component,
)

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@jesserockz", "@kahrendt"]
DEPENDENCIES = ["i2s_audio"]

I2SAudioSpeaker = i2s_audio_ns.class_(
    "I2SAudioSpeaker", cg.Component, speaker.Speaker, I2SAudioOut
)

CONF_BUFFER_DURATION = "buffer_duration"
CONF_DAC_TYPE = "dac_type"
CONF_I2S_COMM_FMT = "i2s_comm_fmt"
CONF_NEVER = "never"

i2s_dac_mode_t = cg.global_ns.enum("i2s_dac_mode_t")
INTERNAL_DAC_OPTIONS = {
    CONF_LEFT: i2s_dac_mode_t.I2S_DAC_CHANNEL_LEFT_EN,
    CONF_RIGHT: i2s_dac_mode_t.I2S_DAC_CHANNEL_RIGHT_EN,
    CONF_STEREO: i2s_dac_mode_t.I2S_DAC_CHANNEL_BOTH_EN,
}

i2s_comm_format_t = cg.global_ns.enum("i2s_comm_format_t")
I2C_COMM_FMT_OPTIONS = {
    "stand_i2s": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_I2S,
    "stand_msb": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_MSB,
    "stand_pcm_short": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_PCM_SHORT,
    "stand_pcm_long": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_PCM_LONG,
    "stand_max": i2s_comm_format_t.I2S_COMM_FORMAT_STAND_MAX,
    "i2s_msb": i2s_comm_format_t.I2S_COMM_FORMAT_I2S_MSB,
    "i2s_lsb": i2s_comm_format_t.I2S_COMM_FORMAT_I2S_LSB,
    "pcm": i2s_comm_format_t.I2S_COMM_FORMAT_PCM,
    "pcm_short": i2s_comm_format_t.I2S_COMM_FORMAT_PCM_SHORT,
    "pcm_long": i2s_comm_format_t.I2S_COMM_FORMAT_PCM_LONG,
}

NO_INTERNAL_DAC_VARIANTS = [esp32.const.VARIANT_ESP32S2]


def validate_esp32_variant(config):
    if config[CONF_DAC_TYPE] != "internal":
        return config
    variant = esp32.get_esp32_variant()
    if variant in NO_INTERNAL_DAC_VARIANTS:
        raise cv.Invalid(f"{variant} does not have an internal DAC")
    return config


BASE_SCHEMA = (
    speaker.SPEAKER_SCHEMA.extend(
        i2s_audio_component_schema(
            I2SAudioSpeaker,
            default_sample_rate=16000,
            default_channel=CONF_MONO,
            default_bits_per_sample="16bit",
        )
    )
    .extend(
        {
            cv.Optional(
                CONF_BUFFER_DURATION, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TIMEOUT, default="500ms"): cv.Any(
                cv.positive_time_period_milliseconds,
                cv.one_of(CONF_NEVER, lower=True),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": BASE_SCHEMA.extend(
                {
                    cv.Required(CONF_MODE): cv.enum(INTERNAL_DAC_OPTIONS, lower=True),
                }
            ),
            "external": BASE_SCHEMA.extend(
                {
                    cv.Required(
                        CONF_I2S_DOUT_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_I2S_COMM_FMT, default="stand_i2s"): cv.enum(
                        I2C_COMM_FMT_OPTIONS, lower=True
                    ),
                }
            ),
        },
        key=CONF_DAC_TYPE,
    ),
    validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_i2s_audio_component(var, config)
    await speaker.register_speaker(var, config)

    if config[CONF_DAC_TYPE] == "internal":
        cg.add(var.set_internal_dac_mode(config[CONF_CHANNEL]))
    else:
        cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
        cg.add(var.set_i2s_comm_fmt(config[CONF_I2S_COMM_FMT]))
    if config[CONF_TIMEOUT] != CONF_NEVER:
        cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_buffer_duration(config[CONF_BUFFER_DURATION]))
