import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@deisterhold"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

neokey_ns = cg.esphome_ns.namespace("neokey")
NeoKeyComponent = neokey_ns.class_(
    "NeoKeyComponent", cg.PollingComponent, i2c.I2CDevice
)


CONF_NEOKEY_ID = "neokey_id"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NeoKeyComponent),
        }
    )
    .extend(cv.polling_component_schema("10ms"))
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add_library("SPI", None)
    cg.add_library("Wire", None)
    cg.add_library("adafruit/Adafruit BusIO", None)
    cg.add_library("adafruit/Adafruit seesaw Library", None)
