import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, ssd1327_base
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@kbx81"]

AUTO_LOAD = ["ssd1327_base"]
DEPENDENCIES = ["spi"]

ssd1327_spi = cg.esphome_ns.namespace("ssd1327_spi")
SPISSD1327 = ssd1327_spi.class_("SPISSD1327", ssd1327_base.SSD1327, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    ssd1327_base.SSD1327_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPISSD1327),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield ssd1327_base.setup_ssd1327(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
