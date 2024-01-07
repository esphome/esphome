import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, wk2168
from esphome.const import CONF_ID

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["wk2168"]
MULTI_CONF = True

wk2168_ns = cg.esphome_ns.namespace("wk2168_spi")
WK2168ComponentSPI = wk2168_ns.class_(
    "WK2168ComponentSPI", wk2168.WK2168Component, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    wk2168.WK2168_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2168ComponentSPI),
        }
    ).extend(spi.spi_device_schema()),
    wk2168.post_check_conf_wk2168,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk2168.register_wk2168(var, config)
    await spi.register_spi_device(var, config)
