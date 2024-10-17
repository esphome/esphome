import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

CONF_HUSB238_ID = "husb238_id"

husb238_ns = cg.esphome_ns.namespace("husb238")
Husb238Component = husb238_ns.class_(
    "Husb238Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.Schema(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Husb238Component),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x08))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
