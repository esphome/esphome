import voluptuous as vol

from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import (CONF_BIT_DEPTH, CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID,
                           CONF_NUM_CHANNELS, CONF_NUM_CHIPS, CONF_UPDATE_ON_BOOT)
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

MY9231OutputComponent = output.output_ns.class_('MY9231OutputComponent', Component)
MULTI_CONF = True

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(MY9231OutputComponent),
    vol.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_NUM_CHANNELS): vol.All(vol.Coerce(int),
                                             vol.Range(3, 1020)),
    vol.Optional(CONF_NUM_CHIPS): vol.All(vol.Coerce(int),
                                          vol.Range(1, 255)),
    vol.Optional(CONF_BIT_DEPTH): cv.one_of(8, 12, 14, 16, int=True),
    vol.Optional(CONF_UPDATE_ON_BOOT): vol.Coerce(bool),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for di in gpio_output_pin_expression(config[CONF_DATA_PIN]):
        yield
    for dcki in gpio_output_pin_expression(config[CONF_CLOCK_PIN]):
        yield
    rhs = App.make_my9231_component(di, dcki)
    my9231 = Pvariable(config[CONF_ID], rhs)
    if CONF_NUM_CHANNELS in config:
        add(my9231.set_num_channels(config[CONF_NUM_CHANNELS]))
    if CONF_NUM_CHIPS in config:
        add(my9231.set_num_chips(config[CONF_NUM_CHIPS]))
    if CONF_BIT_DEPTH in config:
        add(my9231.set_bit_depth(config[CONF_BIT_DEPTH]))
    if CONF_UPDATE_ON_BOOT in config:
        add(my9231.set_update(config[CONF_UPDATE_ON_BOOT]))
    setup_component(my9231, config)


BUILD_FLAGS = '-DUSE_MY9231_OUTPUT'
