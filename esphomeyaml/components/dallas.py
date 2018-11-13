import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ID, CONF_PIN, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable, setup_component, PollingComponent

DallasComponent = sensor.sensor_ns.class_('DallasComponent', PollingComponent)

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(DallasComponent),
    vol.Required(CONF_PIN): pins.input_output_pin,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)])


def to_code(config):
    for conf in config:
        rhs = App.make_dallas_component(conf[CONF_PIN], conf.get(CONF_UPDATE_INTERVAL))
        var = Pvariable(conf[CONF_ID], rhs)
        setup_component(var, conf)


BUILD_FLAGS = '-DUSE_DALLAS_SENSOR'
