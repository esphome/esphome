import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import output
from esphomeyaml.const import (CONF_DATA_PIN, CONF_CLOCK_PIN, CONF_NUM_CHANNELS,
                               CONF_NUM_CHIPS, CONF_BIT_DEPTH, CONF_ID,
                               CONF_UPDATE_ON_BOOT)
from esphomeyaml.helpers import (gpio_output_pin_expression, App, Pvariable,
                                 add, setup_component, Component)

MY9231OutputComponent = output.output_ns.class_('MY9231OutputComponent', Component)


MY9231_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(MY9231OutputComponent),
    vol.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_NUM_CHANNELS): vol.All(vol.Coerce(int),
                                             vol.Range(3, 1020)),
    vol.Optional(CONF_NUM_CHIPS): vol.All(vol.Coerce(int),
                                          vol.Range(1, 255)),
    vol.Optional(CONF_BIT_DEPTH): vol.All(vol.Coerce(int),
                                          cv.one_of(8, 12, 14, 16)),
    vol.Optional(CONF_UPDATE_ON_BOOT): vol.Coerce(bool),
}).extend(cv.COMPONENT_SCHEMA.schema)

CONFIG_SCHEMA = vol.All(cv.ensure_list, [MY9231_SCHEMA])


def to_code(config):
    for conf in config:
        di = None
        for di in gpio_output_pin_expression(conf[CONF_DATA_PIN]):
            yield
        dcki = None
        for dcki in gpio_output_pin_expression(conf[CONF_CLOCK_PIN]):
            yield
        rhs = App.make_my9231_component(di, dcki)
        my9231 = Pvariable(conf[CONF_ID], rhs)
        if CONF_NUM_CHANNELS in conf:
            add(my9231.set_num_channels(conf[CONF_NUM_CHANNELS]))
        if CONF_NUM_CHIPS in conf:
            add(my9231.set_num_chips(conf[CONF_NUM_CHIPS]))
        if CONF_BIT_DEPTH in conf:
            add(my9231.set_bit_depth(conf[CONF_BIT_DEPTH]))
        if CONF_UPDATE_ON_BOOT in conf:
            add(my9231.set_update(conf[CONF_UPDATE_ON_BOOT]))
        setup_component(my9231, conf)


BUILD_FLAGS = '-DUSE_MY9231_OUTPUT'
