import esphome.config_validation as cv
import esphome.codegen as cg

from esphome import pins
from esphome.const import CONF_ID
from esphome.components import microphone

from .. import (
    i2s_audio_ns,
    I2SAudioComponent,
    I2SAudioIn,
    CONF_I2S_AUDIO_ID,
    CONF_I2S_DIN_PIN,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2s_audio"]

I2SAudioMicrophone = i2s_audio_ns.class_(
    "I2SAudioMicrophone", I2SAudioIn, microphone.Microphone, cg.Component
)

CONFIG_SCHEMA = microphone.MICROPHONE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(I2SAudioMicrophone),
        cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
        cv.Required(CONF_I2S_DIN_PIN): pins.internal_gpio_input_pin_number,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])

    cg.add(var.set_din_pin(config[CONF_I2S_DIN_PIN]))

    await microphone.register_microphone(var, config)
