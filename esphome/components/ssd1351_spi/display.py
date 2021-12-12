import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, ssd1351_base
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["ssd1351_base"]
DEPENDENCIES = ["spi"]

ssd1351_spi = cg.esphome_ns.namespace("ssd1351_spi")
SPISSD1351 = ssd1351_spi.class_("SPISSD1351", ssd1351_base.SSD1351, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    ssd1351_base.SSD1351_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPISSD1351),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1351_base.setup_ssd1351(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
