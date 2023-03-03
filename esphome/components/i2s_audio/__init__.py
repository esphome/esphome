import esphome.config_validation as cv
import esphome.codegen as cg

from esphome import pins
from esphome.const import CONF_ID
from esphome.components.esp32 import add_idf_component

# CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_DIN_PIN = "i2s_din_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"

CONF_I2S_AUDIO_ID = "i2s_audio_id"

i2s_audio_ns = cg.esphome_ns.namespace("i2s_audio")
I2SAudioComponent = i2s_audio_ns.class_("I2SAudioComponent", cg.Component)
I2SAudioIn = i2s_audio_ns.class_("I2SAudioIn", cg.Parented.template(I2SAudioComponent))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(I2SAudioComponent),
        cv.Required(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
    cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))

    # add_idf_component("esp-adf", "https://github.com/espressif/esp-adf.git", "v2.4")
