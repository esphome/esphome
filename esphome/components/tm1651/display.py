import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_ID, CONF_CLK_PIN

tm1651_ns = cg.esphome_ns.namespace('tm1651')
TM1651Display = tm1651_ns.class_('TM1651Display', cg.Component)

CONF_DIO_PIN = 'dio_pin'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TM1651Display),
    cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DIO_PIN): pins.gpio_output_pin_schema,
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    clk_pin = yield cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    dio_pin = yield cg.gpio_pin_expression(config[CONF_DIO_PIN])
    cg.add(var.set_dio_pin(dio_pin))

    # https://platformio.org/lib/show/6865/TM1651
    cg.add_library('6865', '1.0.0')
