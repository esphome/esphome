import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN
from esphome.types import ConfigType

CODEOWNERS = ["@P4uLT"]
CONF_ES8388_ID = "es8388_id"

es8388_ns = cg.esphome_ns.namespace("es8388")

ES8388 = es8388_ns.class_("ES8388", AudioDac, cg.Component, i2c.I2CDevice)

es8388_mic_gain = es8388_ns.enum("ES8388MicGain")
ES8388_MIC_GAIN_ENUM = es8388_mic_gain.options(
    "0DB",
    "3DB",
    "6DB",
    "9DB",
    "12DB",
    "15DB",
    "18DB",
    "21DB",
    "24DB",
    prefix="ES8388_MIC_GAIN_",
)

DEPENDENCIES = ["i2c"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8388),
            cv.Optional(CONF_MIC_GAIN, default="0DB"): cv.enum(
                ES8388_MIC_GAIN_ENUM, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
