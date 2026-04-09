import esphome.codegen as cg
from esphome.components import st25r, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["st25r"]
CODEOWNERS = ["@JohnMcLear"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

st25r_spi_ns = cg.esphome_ns.namespace("st25r_spi")
ST25RSpi = st25r_spi_ns.class_("ST25RSpi", st25r.ST25R, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    st25r.ST25R_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST25RSpi),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await st25r.setup_st25r(var, config)
    await spi.register_spi_device(var, config)
