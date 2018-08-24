import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, variable

MakeWiFiSignalSensor = Application.MakeWiFiSignalSensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeWiFiSignalSensor),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    rhs = App.make_wifi_signal_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Pwifi, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_WIFI_SIGNAL_SENSOR'
