import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL, CONF_ADDRESS, CONF_ACCURACY
from esphomeyaml.helpers import App, variable, RawExpression, add

DEPENDENCIES = ['i2c']

SHT_ACCURACIES = {
    'LOW': 'sensor::SHT3XD_ACCURACY_LOW',
    'MEDIUM': 'sensor::SHT3XD_ACCURACY_MEDIUM',
    'HIGH': 'sensor::SHT3XD_ACCURACY_HIGH',
}

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('sht3xd'): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): MQTT_SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_ADDRESS, default=0x44): cv.i2c_address,
    vol.Optional(CONF_ACCURACY): vol.All(vol.Upper, vol.Any(*SHT_ACCURACIES)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_sht3xd_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    sht3xd = variable('Application::MakeSHT3XDSensor', config[CONF_ID], rhs)

    if CONF_ACCURACY in config:
        constant = RawExpression(SHT_ACCURACIES[config[CONF_ACCURACY]])
        add(sht3xd.Psht3xd.set_accuracy(constant))

    sensor.setup_sensor(sht3xd.Psht3xd.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(sht3xd.Pmqtt_temperature, config[CONF_TEMPERATURE])

    sensor.setup_sensor(sht3xd.PPsht3xd.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_mqtt_sensor_component(sht3xd.Pmqtt_humidity, config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_SHT3XD'
