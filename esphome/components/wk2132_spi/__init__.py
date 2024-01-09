import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, wk_base
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["wk_base"]
MULTI_CONF = True

wk2132_spi_ns = cg.esphome_ns.namespace("wk2132_spi")
WK2132ComponentSPI = wk2132_spi_ns.class_(
    "WK2132ComponentSPI", wk_base.WKBaseComponent, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    wk_base.WK2132_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2132ComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    wk_base.post_check_conf_wk_base,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk_base.register_wk_base(var, config)
    await spi.register_spi_device(var, config)
