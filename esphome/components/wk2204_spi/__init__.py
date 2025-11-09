import esphome.codegen as cg
from esphome.components import spi, weikai
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["weikai", "weikai_spi"]
MULTI_CONF = True

weikai_spi_ns = cg.esphome_ns.namespace("weikai_spi")
WeikaiComponentSPI = weikai_spi_ns.class_(
    "WeikaiComponentSPI", weikai.WeikaiComponent, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    weikai.WKBASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeikaiComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    weikai.check_channel_max_4,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_name(str(config[CONF_ID])))
    await weikai.register_weikai(var, config)
    await spi.register_spi_device(var, config)
