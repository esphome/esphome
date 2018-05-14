import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, variable

DEPENDENCIES = ['i2c']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('htu21d'): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): MQTT_SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_htu21d_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    htu21d = variable('Application::MakeHTU21DSensor', config[CONF_ID], rhs)
    sensor.setup_sensor(htu21d.Phtu21d.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(htu21d.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(htu21d.Phtu21d.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_mqtt_sensor_component(htu21d.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_HTU21D_SENSOR'
