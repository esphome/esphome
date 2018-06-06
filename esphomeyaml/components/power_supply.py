import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_ENABLE_TIME, CONF_ID, CONF_KEEP_ON_TIME, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, gpio_output_pin_expression

PowerSupplyComponent = esphomelib_ns.PowerSupplyComponent

POWER_SUPPLY_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(PowerSupplyComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_ENABLE_TIME): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_KEEP_ON_TIME): cv.positive_time_period_milliseconds,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [POWER_SUPPLY_SCHEMA])


def to_code(config):
    for conf in config:
        pin = None
        for pin in gpio_output_pin_expression(conf[CONF_PIN]):
            yield
        rhs = App.make_power_supply(pin)
        psu = Pvariable(conf[CONF_ID], rhs)
        if CONF_ENABLE_TIME in conf:
            add(psu.set_enable_time(conf[CONF_ENABLE_TIME]))
        if CONF_KEEP_ON_TIME in conf:
            add(psu.set_keep_on_time(conf[CONF_KEEP_ON_TIME]))


BUILD_FLAGS = '-DUSE_OUTPUT'
