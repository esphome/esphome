import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['i2c']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('dht_sensor', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): sensor.SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})

MakeDHT12Sensor = Application.MakeDHT12Sensor


def to_code(config):
    rhs = App.make_dht12_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                config[CONF_HUMIDITY][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    dht = variable(MakeDHT12Sensor, config[CONF_MAKE_ID], rhs)

    sensor.setup_sensor(dht.Pdht.Pget_temperature_sensor(), dht.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pdht.Pget_humidity_sensor(), dht.Pmqtt_humidity,
                        config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_DHT12_SENSOR'
