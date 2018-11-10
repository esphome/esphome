import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Application, variable, setup_component

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

MakeESP32HallSensor = Application.struct('MakeESP32HallSensor')
ESP32HallSensor = sensor.sensor_ns.class_('ESP32HallSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ESP32HallSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeESP32HallSensor),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_esp32_hall_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Phall, make.Pmqtt, config)
    setup_component(make.Phall, config)


BUILD_FLAGS = '-DUSE_ESP32_HALL_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
