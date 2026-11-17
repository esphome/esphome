import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]
DOMAIN = "husb238"
MULTI_CONF = True

CONF_HUSB238_ID = "husb238_id"

husb238_ns = cg.esphome_ns.namespace("husb238")
Husb238Component = husb238_ns.class_(
    "Husb238Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Husb238Component),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x08))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)
