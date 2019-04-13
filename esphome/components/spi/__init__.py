from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CLK_PIN, CONF_ID, CONF_MISO_PIN, CONF_MOSI_PIN


spi_ns = cg.esphome_ns.namespace('spi')
SPIComponent = spi_ns.class_('SPIComponent', cg.Component)
SPIDevice = spi_ns.class_('SPIDevice')
MULTI_CONF = True

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(SPIComponent),
    cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
}), cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN))


def to_code(config):
    clk = yield cg.gpio_pin_expression(config[CONF_CLK_PIN])
    miso = mosi = cg.nullptr
    if CONF_MISO_PIN in config:
        miso = yield cg.gpio_pin_expression(config[CONF_MISO_PIN])
    if CONF_MOSI_PIN in config:
        mosi = yield cg.gpio_pin_expression(config[CONF_MOSI_PIN])

    var = cg.new_Pvariable(config[CONF_ID], clk, miso, mosi)
    yield cg.register_component(var, config)
