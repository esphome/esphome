import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, weikai
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["weikai"]
MULTI_CONF = True

weikai_ns = cg.esphome_ns.namespace("weikai")
WeikaiComponentSPI = weikai_ns.class_(
    "WeikaiComponentSPI", weikai.WeikaiComponent, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    weikai.WKBASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeikaiComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    weikai.check_channel_max_2,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add_build_flag("-DUSE_SPI_BUS")
    cg.add(var.set_name(str(config[CONF_ID])))
    await weikai.register_weikai(var, config)
    await spi.register_spi_device(var, config)
