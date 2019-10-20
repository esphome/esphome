import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, ssd1325_base
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

AUTO_LOAD = ['ssd1325_base']
DEPENDENCIES = ['spi']

ssd1325_spi = cg.esphome_ns.namespace('ssd1325_spi')
SPISSD1325 = ssd1325_spi.class_('SPISSD1325', ssd1325_base.SSD1325, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(ssd1325_base.SSD1325_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(SPISSD1325),
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA).extend(spi.SPI_DEVICE_SCHEMA),
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield ssd1325_base.setup_ssd1036(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
