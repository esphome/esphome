import voluptuous as vol

from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.cpp_generator import variable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Application

MakeWiFiSignalSensor = Application.struct('MakeWiFiSignalSensor')
WiFiSignalSensor = sensor.sensor_ns.class_('WiFiSignalSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(WiFiSignalSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeWiFiSignalSensor),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_wifi_signal_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    wifi = make.Pwifi

    sensor.setup_sensor(wifi, make.Pmqtt, config)
    setup_component(wifi, config)


BUILD_FLAGS = '-DUSE_WIFI_SIGNAL_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
