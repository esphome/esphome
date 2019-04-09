import voluptuous as vol

from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import (CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID,
                           CONF_NUM_CHANNELS, CONF_NUM_CHIPS)
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, Component

SM16716OutputComponent = output.output_ns.class_('SM16716OutputComponent', Component)
MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(SM16716OutputComponent),
    vol.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_NUM_CHANNELS): vol.All(vol.Coerce(int),
                                             vol.Range(3, 255)),
    vol.Optional(CONF_NUM_CHIPS): vol.All(vol.Coerce(int),
                                          vol.Range(1, 85)),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for data in gpio_output_pin_expression(config[CONF_DATA_PIN]):
        yield
    for clock in gpio_output_pin_expression(config[CONF_CLOCK_PIN]):
        yield
    rhs = App.make_sm16716_component(data, clock)
    sm16716 = Pvariable(config[CONF_ID], rhs)
    if CONF_NUM_CHANNELS in config:
        add(sm16716.set_num_channels(config[CONF_NUM_CHANNELS]))
    if CONF_NUM_CHIPS in config:
        add(sm16716.set_num_chips(config[CONF_NUM_CHIPS]))
    setup_component(sm16716, config)


BUILD_FLAGS = '-DUSE_SM16716_OUTPUT'
