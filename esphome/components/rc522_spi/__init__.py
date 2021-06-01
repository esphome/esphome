import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, rc522
from esphome.const import CONF_ID

CODEOWNERS = ["@glmnet"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["rc522"]
MULTI_CONF = True

rc522_spi_ns = cg.esphome_ns.namespace("rc522_spi")
RC522Spi = rc522_spi_ns.class_("RC522Spi", rc522.RC522, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    rc522.RC522_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RC522Spi),
        }
    ).extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await rc522.setup_rc522(var, config)
    await spi.register_spi_device(var, config)


def validate(config, item_config):
    # validate given SPI hub is suitable for rc522_spi, it needs both miso and mosi
    spi.validate_device("rc522_spi", config, item_config, True, True)
