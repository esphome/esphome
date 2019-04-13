from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (CONF_BIT_DEPTH, CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID,
                           CONF_NUM_CHANNELS, CONF_NUM_CHIPS, CONF_UPDATE_ON_BOOT)


MY9231OutputComponent = output.output_ns.class_('MY9231OutputComponent', Component)
MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(MY9231OutputComponent),
    cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_NUM_CHANNELS): cv.All(cv.Coerce(int),
                                             cv.Range(3, 1020)),
    cv.Optional(CONF_NUM_CHIPS): cv.All(cv.Coerce(int),
                                          cv.Range(1, 255)),
    cv.Optional(CONF_BIT_DEPTH): cv.one_of(8, 12, 14, 16, int=True),
    cv.Optional(CONF_UPDATE_ON_BOOT): cv.Coerce(bool),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    di = yield gpio_output_pin_expression(config[CONF_DATA_PIN])
    dcki = yield gpio_output_pin_expression(config[CONF_CLOCK_PIN])
    rhs = App.make_my9231_component(di, dcki)
    my9231 = Pvariable(config[CONF_ID], rhs)
    if CONF_NUM_CHANNELS in config:
        cg.add(my9231.set_num_channels(config[CONF_NUM_CHANNELS]))
    if CONF_NUM_CHIPS in config:
        cg.add(my9231.set_num_chips(config[CONF_NUM_CHIPS]))
    if CONF_BIT_DEPTH in config:
        cg.add(my9231.set_bit_depth(config[CONF_BIT_DEPTH]))
    if CONF_UPDATE_ON_BOOT in config:
        cg.add(my9231.set_update(config[CONF_UPDATE_ON_BOOT]))
    register_component(my9231, config)


BUILD_FLAGS = '-DUSE_MY9231_OUTPUT'
