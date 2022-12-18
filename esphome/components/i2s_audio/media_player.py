import esphome.codegen as cg
from esphome.components import media_player, esp32
import esphome.config_validation as cv

from esphome import pins

from esphome.const import CONF_ID, CONF_MODE
from esphome.core import CORE

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

i2s_audio_ns = cg.esphome_ns.namespace("i2s_audio")

I2SAudioMediaPlayer = i2s_audio_ns.class_(
    "I2SAudioMediaPlayer", cg.Component, media_player.MediaPlayer
)

i2s_dac_mode_t = cg.global_ns.enum("i2s_dac_mode_t")

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"
CONF_MUTE_PIN = "mute_pin"
CONF_AUDIO_ID = "audio_id"
CONF_DAC_TYPE = "dac_type"

INTERNAL_DAC_OPTIONS = {
    "left": i2s_dac_mode_t.I2S_DAC_CHANNEL_LEFT_EN,
    "right": i2s_dac_mode_t.I2S_DAC_CHANNEL_RIGHT_EN,
    "stereo": i2s_dac_mode_t.I2S_DAC_CHANNEL_BOTH_EN,
}

EXTERNAL_DAC_OPTIONS = ["mono", "stereo"]

NO_INTERNAL_DAC_VARIANTS = [esp32.const.VARIANT_ESP32S2]


def validate_esp32_variant(config):
    if config[CONF_DAC_TYPE] != "internal":
        return config
    variant = esp32.get_esp32_variant()
    if variant in NO_INTERNAL_DAC_VARIANTS:
        raise cv.Invalid(f"{variant} does not have an internal DAC")
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "internal": cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(I2SAudioMediaPlayer),
                    cv.Required(CONF_MODE): cv.enum(INTERNAL_DAC_OPTIONS, lower=True),
                }
            )
            .extend(media_player.MEDIA_PLAYER_SCHEMA)
            .extend(cv.COMPONENT_SCHEMA),
            "external": cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(I2SAudioMediaPlayer),
                    cv.Required(
                        CONF_I2S_DOUT_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Required(
                        CONF_I2S_BCLK_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Required(
                        CONF_I2S_LRCLK_PIN
                    ): pins.internal_gpio_output_pin_number,
                    cv.Optional(CONF_MUTE_PIN): pins.gpio_output_pin_schema,
                    cv.Optional(CONF_MODE, default="mono"): cv.one_of(
                        *EXTERNAL_DAC_OPTIONS, lower=True
                    ),
                }
            )
            .extend(media_player.MEDIA_PLAYER_SCHEMA)
            .extend(cv.COMPONENT_SCHEMA),
        },
        key=CONF_DAC_TYPE,
    ),
    cv.only_with_arduino,
    validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    if config[CONF_DAC_TYPE] == "internal":
        cg.add(var.set_internal_dac_mode(config[CONF_MODE]))
    else:
        cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
        cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
        cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))
        if CONF_MUTE_PIN in config:
            pin = await cg.gpio_pin_expression(config[CONF_MUTE_PIN])
            cg.add(var.set_mute_pin(pin))
        cg.add(var.set_external_dac_channels(2 if config[CONF_MODE] == "stereo" else 1))

    if CORE.is_esp32:
        cg.add_library("WiFiClientSecure", None)
        cg.add_library("HTTPClient", None)
        cg.add_library("esphome/ESP32-audioI2S", "2.0.6")
        cg.add_build_flag("-DAUDIO_NO_SD_FS")
