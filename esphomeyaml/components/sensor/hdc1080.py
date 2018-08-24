import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['i2c']

MakeHDC1080Sensor = Application.MakeHDC1080Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHDC1080Sensor),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    rhs = App.make_hdc1080_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                  config[CONF_HUMIDITY][CONF_NAME],
                                  config.get(CONF_UPDATE_INTERVAL))
    hdc1080 = variable(config[CONF_MAKE_ID], rhs)

    sensor.setup_sensor(hdc1080.Phdc1080.Pget_temperature_sensor(),
                        hdc1080.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(hdc1080.Phdc1080.Pget_humidity_sensor(), hdc1080.Pmqtt_humidity,
                        config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_HDC1080_SENSOR'
