import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, wk2132
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["wk2132"]
MULTI_CONF = True

wk2132_ns = cg.esphome_ns.namespace("wk2132_spi")
WK2132ComponentSPI = wk2132_ns.class_(
    "WK2132ComponentSPI", wk2132.WK2132Component, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    wk2132.WK2132_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2132ComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    wk2132.post_check_conf_wk2132,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk2132.register_wk2132(var, config)
    await spi.register_spi_device(var, config)
