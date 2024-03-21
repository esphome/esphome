import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "voltage_sampler"]
MULTI_CONF = True

ads1220_ns = cg.esphome_ns.namespace("ads1220")
ADS1220Component = ads1220_ns.class_("ADS1220Component", cg.Component, spi.SPIDevice)

CONF_CONTINUOUS_MODE = "continuous_mode"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1220Component),
            cv.Optional(CONF_CONTINUOUS_MODE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(None))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_continuous_mode(config[CONF_CONTINUOUS_MODE]))