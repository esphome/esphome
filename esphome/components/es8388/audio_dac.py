import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@P4uLT"]
CONF_ES8388_ID = "es8388_id"

es8388_ns = cg.esphome_ns.namespace("es8388")

ES8388 = es8388_ns.class_("ES8388", AudioDac, cg.Component, i2c.I2CDevice)

DEPENDENCIES = ["i2c"]

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(ES8388)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)
