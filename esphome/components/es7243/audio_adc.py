import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_adc import AudioAdc
from esphome.components.i2s_audio import (
    CONF_I2S_AUDIO_ID,
    CONF_I2S_MCLK_PIN,
    I2SAudioComponent,
)
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Chorty"]
DEPENDENCIES = ["i2c", "i2s_audio"]

es7243_ns = cg.esphome_ns.namespace("es7243")
ES7243 = es7243_ns.class_("ES7243", AudioAdc, cg.Component, i2c.I2CDevice)

# Gains the chip's register 0x08 can express, from the ESP-ADF es7243 driver
ES7243_MIC_GAINS = [1, 3.5, 18, 20.5, 22.5, 24.5, 25, 27]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7243),
            cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
            cv.Optional(CONF_MIC_GAIN, default="27db"): cv.All(
                cv.decibel, cv.one_of(*ES7243_MIC_GAINS)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x13))
)


def _validate_mclk_pin(config: ConfigType) -> None:
    """The chip only answers on I2C after it has seen MCLK, so the bus must drive that pin."""
    i2s_id = config[CONF_I2S_AUDIO_ID]
    if CONF_I2S_MCLK_PIN not in fv.get_declaring_config(i2s_id):
        raise cv.Invalid(f"es7243 needs {CONF_I2S_MCLK_PIN} on i2s_audio {i2s_id}")


FINAL_VALIDATE_SCHEMA = _validate_mclk_pin


async def to_code(config: ConfigType) -> None:
    i2s = await cg.get_variable(config[CONF_I2S_AUDIO_ID])
    var = await i2c.new_i2c_component(config, i2s)

    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
