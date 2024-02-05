import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, wk_base
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["wk_base"]
MULTI_CONF = True

wk_base_ns = cg.esphome_ns.namespace("wk_base")
WKBaseComponentSPI = wk_base_ns.class_(
    "WKBaseComponentSPI", wk_base.WKBaseComponent, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    wk_base.WKBASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WKBaseComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    wk_base.check_channel_max_4,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_build_flag("-DUSE_SPI_BUS")
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk_base.register_wk_base(var, config)
    await spi.register_spi_device(var, config)
