from esphome import pins
import esphome.codegen as cg
from esphome.components import spi, ssd1331_base
import esphome.config_validation as cv
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["ssd1331_base"]
DEPENDENCIES = ["spi"]

ssd1331_spi = cg.esphome_ns.namespace("ssd1331_spi")
SPISSD1331 = ssd1331_spi.class_("SPISSD1331", ssd1331_base.SSD1331, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    ssd1331_base.SSD1331_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPISSD1331),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "ssd1331_spi", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ssd1331_base.setup_ssd1331(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
