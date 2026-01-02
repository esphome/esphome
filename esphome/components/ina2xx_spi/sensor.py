import esphome.codegen as cg
from esphome.components import ina2xx_base, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL

AUTO_LOAD = ["ina2xx_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["spi"]

ina2xx_spi = cg.esphome_ns.namespace("ina2xx_spi")
INA2XX_SPI = ina2xx_spi.class_("INA2XXSPI", ina2xx_base.INA2XX, spi.SPIDevice)

INAModel = ina2xx_base.ina2xx_base_ns.enum("INAModel")
INA_MODELS = {
    "INA229": INAModel.INA_229,
    "INA239": INAModel.INA_239,
}

CONFIG_SCHEMA = cv.All(
    ina2xx_base.INA2XX_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(INA2XX_SPI),
            cv.Required(CONF_MODEL): cv.enum(INA_MODELS, upper=True),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True)),
    ina2xx_base.validate_model_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ina2xx_base.setup_ina2xx(var, config)
    await spi.register_spi_device(var, config)
