import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ENABLE_TIME, CONF_ID, CONF_KEEP_ON_TIME, CONF_PIN

power_supply_ns = cg.esphome_ns.namespace('power_supply')
PowerSupply = power_supply_ns.class_('PowerSupply', cg.Component)
MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(PowerSupply),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_ENABLE_TIME, default='20ms'): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_KEEP_ON_TIME, default='10s'): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])

    var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_ENABLE_TIME],
                           config[CONF_KEEP_ON_TIME])
    yield cg.register_component(var, config)
    cg.add_define('USE_POWER_SUPPLY')
