import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, variable

DEPENDENCIES = ['i2c']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('dht_sensor'): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): MQTT_SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
})


def to_code(config):
    rhs = App.make_hdc1080_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                  config[CONF_HUMIDITY][CONF_NAME],
                                  config.get(CONF_UPDATE_INTERVAL))
    hdc1080 = variable('Application::MakeHDC1080Sensor', config[CONF_ID], rhs)
    sensor.setup_sensor(hdc1080.Phdc1080.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(hdc1080.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(hdc1080.Phdc1080.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_mqtt_sensor_component(hdc1080.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_HDC1080_SENSOR'
