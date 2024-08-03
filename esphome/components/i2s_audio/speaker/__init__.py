from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, speaker
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from .. import (
    CONF_I2S_DOUT_PIN,
    I2SAudioOut,
    I2SAudioSchema,
    i2s_audio_ns,
    register_i2saudio,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2s_audio"]

I2SAudioSpeaker = i2s_audio_ns.class_(
    "I2SAudioSpeaker", cg.Component, speaker.Speaker, I2SAudioOut
)


CONF_DAC_TYPE = "dac_type"

i2s_dac_mode_t = cg.global_ns.enum("i2s_dac_mode_t")
INTERNAL_DAC_OPTIONS = {
    "left": i2s_dac_mode_t.I2S_DAC_CHANNEL_LEFT_EN,
    "right": i2s_dac_mode_t.I2S_DAC_CHANNEL_RIGHT_EN,
    "stereo": i2s_dac_mode_t.I2S_DAC_CHANNEL_BOTH_EN,
}


NO_INTERNAL_DAC_VARIANTS = [esp32.const.VARIANT_ESP32S2]


def validate_esp32_variant(config):
    if config[CONF_DAC_TYPE] != "internal":
        return config
    variant = esp32.get_esp32_variant()
    if variant in NO_INTERNAL_DAC_VARIANTS:
        raise cv.Invalid(f"{variant} does not have an internal DAC")
    return config


BASE_SCHEMA = speaker.SPEAKER_SCHEMA.extend(
    I2SAudioSchema(I2SAudioSpeaker, 16000, "stereo", "16bit")
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": BASE_SCHEMA.extend({}),
            "external": BASE_SCHEMA.extend(
                {
                    cv.Required(
                        CONF_I2S_DOUT_PIN
                    ): pins.internal_gpio_output_pin_number,
                }
            ),
        },
        key=CONF_DAC_TYPE,
    ),
    validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_i2saudio(var, config)
    await speaker.register_speaker(var, config)

    if config[CONF_DAC_TYPE] == "internal":
        cg.add(var.set_internal_dac_mode(config[CONF_CHANNEL]))
    else:
        cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
