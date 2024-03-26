import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_DC_PIN,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "voltage_sampler"]
MULTI_CONF = True

ads1220_ns = cg.esphome_ns.namespace("ads1220")
ADS1220Component = ads1220_ns.class_("ADS1220Component", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS1220Component),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(None))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    drdy = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_drdy_pin(drdy))    
