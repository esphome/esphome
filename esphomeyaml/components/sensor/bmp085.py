import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_NAME, \
    CONF_PRESSURE, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, HexIntLiteral, add, variable

DEPENDENCIES = ['i2c']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('bmp085_sensor'): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): MQTT_SENSOR_SCHEMA,
    vol.Required(CONF_PRESSURE): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_bmp085_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    bmp = variable('Application::MakeBMP085Sensor', config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(bmp.Pbmp.set_address(HexIntLiteral(config[CONF_ADDRESS])))
    sensor.setup_sensor(bmp.Pbmp.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(bmp.Pmqtt_temperature, config[CONF_TEMPERATURE])
    sensor.setup_sensor(bmp.Pbmp.Pget_pressure_sensor(), config[CONF_PRESSURE])
    sensor.setup_mqtt_sensor_component(bmp.Pmqtt_pressure, config[CONF_PRESSURE])


BUILD_FLAGS = '-DUSE_BMP085_SENSOR'
