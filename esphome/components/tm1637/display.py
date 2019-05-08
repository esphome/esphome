import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_DATA_PINS, CONF_ENABLE_PIN, CONF_RS_PIN, CONF_RW_PIN, CONF_ID


tm1637_ns = cg.esphome_ns.namespace('tm1637')
TM1637Display = tm1637_ns.class_('TM1637Display', cg.PollingComponent)

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TM1637Display),

    cv.Optional(CONF_INTENSITY, default=15): cv.All(cv.uint8_t, cv.Range(min=0, max=15)),
    cv.Required(CONF_RS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RW_PIN): pins.gpio_output_pin_schema,
}).extend(cv.polling_component_schema('1s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield display.register_display(var, config)

    enable = yield cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable))

    rs = yield cg.gpio_pin_expression(config[CONF_RS_PIN])
    cg.add(var.set_rs_pin(rs))

    if CONF_RW_PIN in config:
        rw = yield cg.gpio_pin_expression(config[CONF_RW_PIN])
        cg.add(var.set_rw_pin(rw))
