import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, hp303b
from esphome.const import CONF_ID

AUTO_LOAD = ["hp303b"]
CODEOWNERS = ["@max246"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True


hp303b_spi_ns = cg.esphome_ns.namespace("hp303b_spi")
HP303BComponentSPI = hp303b_spi_ns.class_(
    "HP303BComponentSPI", hp303b.HP303BComponent, spi.SPIDevice
)

CONFIG_SCHEMA = cv.All(
    hp303b.HB303B_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HP303BComponentSPI),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    print(config)
    var = cg.new_Pvariable(config[CONF_ID])
    await hp303b.setup_hp303b(var, config)
    await spi.register_spi_device(var, config)
