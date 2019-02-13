import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DallasComponent = sensor.sensor_ns.class_('DallasComponent', PollingComponent)
MULTI_CONF = True

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(DallasComponent),
    vol.Required(CONF_PIN): pins.input_pullup_pin,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_dallas_component(config[CONF_PIN], config.get(CONF_UPDATE_INTERVAL))
    var = Pvariable(config[CONF_ID], rhs)
    setup_component(var, config)


BUILD_FLAGS = '-DUSE_DALLAS_SENSOR'
