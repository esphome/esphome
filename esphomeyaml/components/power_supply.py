import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_ENABLE_TIME, CONF_ID, CONF_KEEP_ON_TIME, CONF_PIN
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, gpio_output_pin_expression

POWER_SUPPLY_SCHEMA = cv.REQUIRED_ID_SCHEMA.extend({
    vol.Required(CONF_PIN): pins.GPIO_OUTPUT_PIN_SCHEMA,
    vol.Optional(CONF_ENABLE_TIME): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_KEEP_ON_TIME): cv.positive_time_period_milliseconds,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [POWER_SUPPLY_SCHEMA])

PowerSupplyComponent = esphomelib_ns.PowerSupplyComponent


def to_code(config):
    for conf in config:
        rhs = App.make_power_supply(gpio_output_pin_expression(conf[CONF_PIN]))
        psu = Pvariable(PowerSupplyComponent, conf[CONF_ID], rhs)
        if CONF_ENABLE_TIME in conf:
            add(psu.set_enable_time(conf[CONF_ENABLE_TIME]))
        if CONF_KEEP_ON_TIME in conf:
            add(psu.set_keep_on_time(conf[CONF_KEEP_ON_TIME]))


BUILD_FLAGS = '-DUSE_OUTPUT'
