import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID
from esphome import pins

DEPENDENCIES = ["spi"]
# MULTI_CONF = True
CODEOWNERS = ["@endym"]
CONF_LOAD_PIN = "load_pin"

max6921_ns = cg.esphome_ns.namespace("max6921")
MAX6921 = max6921_ns.class_("MAX6921", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MAX6921),
        cv.Required(CONF_LOAD_PIN): pins.gpio_input_pin_schema,
    }
).extend(spi.spi_device_schema(cs_pin_required=False))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_LOAD_PIN])
    cg.add(var.set_load_pin(pin))
