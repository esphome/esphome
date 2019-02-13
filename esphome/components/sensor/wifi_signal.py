import voluptuous as vol

from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

WiFiSignalSensor = sensor.sensor_ns.class_('WiFiSignalSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(WiFiSignalSensor),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_wifi_signal_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    wifi = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(wifi, config)
    setup_component(wifi, config)


BUILD_FLAGS = '-DUSE_WIFI_SIGNAL_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
