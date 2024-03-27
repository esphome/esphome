import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ina2xx_base, spi
from esphome.const import CONF_ID

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["ina2xx_base"]
DEPENDENCIES = ["spi"]

ina2xx_spi = cg.esphome_ns.namespace("ina2xx_spi")
INA2XX_SPI = ina2xx_spi.class_("INA2XXSPI", ina2xx_base.INA2XX, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    ina2xx_base.INA2XX_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(INA2XX_SPI),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ina2xx_base.setup_ina2xx(var, config)
    await spi.register_spi_device(var, config)
