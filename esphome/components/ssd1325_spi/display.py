from esphome import pins
import esphome.codegen as cg
from esphome.components import spi, ssd1325_base
import esphome.config_validation as cv
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["ssd1325_base"]
DEPENDENCIES = ["spi"]

ssd1325_spi = cg.esphome_ns.namespace("ssd1325_spi")
SPISSD1325 = ssd1325_spi.class_("SPISSD1325", ssd1325_base.SSD1325, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    ssd1325_base.SSD1325_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPISSD1325),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "ssd1325_spi", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1325_base.setup_ssd1325(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
