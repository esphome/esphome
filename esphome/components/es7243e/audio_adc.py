import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_adc import AudioAdc
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MIC_GAIN

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["i2c"]

es7243e_ns = cg.esphome_ns.namespace("es7243e")
ES7243E = es7243e_ns.class_("ES7243E", AudioAdc, cg.Component, i2c.I2CDevice)

ES7243E_MIC_GAINS = [0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 34.5, 36, 37.5]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES7243E),
            cv.Optional(CONF_MIC_GAIN, default="24db"): cv.All(
                cv.decibel, cv.one_of(*ES7243E_MIC_GAINS)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
