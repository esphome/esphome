import esphome.codegen as cg
from esphome.components import spi, st25r300
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["st25r300"]
CODEOWNERS = ["@JohnMcLear"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

st25r300_spi_ns = cg.esphome_ns.namespace("st25r300_spi")
ST25R300Spi = st25r300_spi_ns.class_("ST25R300Spi", st25r300.ST25R300, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    st25r300.ST25R300_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST25R300Spi),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await st25r300.setup_st25r300(var, config)
    await spi.register_spi_device(var, config)
