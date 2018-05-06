import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_ID, CONF_PIN, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable

DALLAS_COMPONENT_CLASS = 'sensor::DallasComponent'

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID('dallas'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.input_output_pin,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
})])


def to_code(config):
    for conf in config:
        rhs = App.make_dallas_component(conf[CONF_PIN], conf.get(CONF_UPDATE_INTERVAL))
        Pvariable(DALLAS_COMPONENT_CLASS, conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_DALLAS_SENSOR'
