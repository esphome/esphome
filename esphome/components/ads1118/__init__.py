import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "voltage_sampler"]
MULTI_CONF = True

CONF_ADS1118 = "ads1118"

ads1118_ns = cg.esphome_ns.namespace("ads1118")
ADS1118 = ads1118_ns.class_("ADS1118", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ADS1118),
    }
).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
