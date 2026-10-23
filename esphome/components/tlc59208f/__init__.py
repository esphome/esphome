import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
DOMAIN = "tlc59208f"
MULTI_CONF = True

tlc59208f_ns = cg.esphome_ns.namespace("tlc59208f")
TLC59208FOutput = tlc59208f_ns.class_("TLC59208FOutput", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TLC59208FOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)
