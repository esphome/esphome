import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import CONF_ID

MULTI_CONF = True
CONF_ATM90E32_ID = "atm90e32_id"

CODEOWNERS = ["@descipher"]

atm90e32_ns = cg.esphome_ns.namespace("atm90e32")
ATM90E32Component = atm90e32_ns.class_(
    "ATM90E32Component", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ATM90E32Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
