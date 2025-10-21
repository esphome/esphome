import esphome.codegen as cg
from esphome.components import pn7160, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["pn7160"]
CODEOWNERS = ["@kbx81", "@jesserockz"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

pn7160_spi_ns = cg.esphome_ns.namespace("pn7160_spi")
PN7160Spi = pn7160_spi_ns.class_("PN7160Spi", pn7160.PN7160, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    pn7160.PN7160_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN7160Spi),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await pn7160.setup_pn7160(var, config)
    await spi.register_spi_device(var, config)
