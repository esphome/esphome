import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["output"]
CODEOWNERS = ["@NickB1"]
DEPENDENCIES = ["i2c"]
DOMAIN = "dac7678"
MULTI_CONF = True

dac7678_ns = cg.esphome_ns.namespace("dac7678")
DAC7678Output = dac7678_ns.class_("DAC7678Output", cg.Component, i2c.I2CDevice)
CONF_INTERNAL_REFERENCE = "internal_reference"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DAC7678Output),
            cv.Optional(CONF_INTERNAL_REFERENCE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config: ConfigType) -> MockObj:
    var = await i2c.new_i2c_component(config)
    cg.add(var.set_internal_reference(config[CONF_INTERNAL_REFERENCE]))
    return var
