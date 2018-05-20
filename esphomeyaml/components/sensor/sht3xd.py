import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ACCURACY, CONF_ADDRESS, CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

SHT_ACCURACIES = {
    'LOW': sensor.sensor_ns.SHT3XD_ACCURACY_LOW,
    'MEDIUM': sensor.sensor_ns.SHT3XD_ACCURACY_MEDIUM,
    'HIGH': sensor.sensor_ns.SHT3XD_ACCURACY_HIGH,
}

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('sht3xd', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_TEMPERATURE): sensor.SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_ADDRESS, default=0x44): cv.i2c_address,
    vol.Optional(CONF_ACCURACY): vol.All(vol.Upper, cv.one_of(*SHT_ACCURACIES)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})

MakeSHT3XDSensor = Application.MakeSHT3XDSensor


def to_code(config):
    rhs = App.make_sht3xd_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    sht3xd = variable(MakeSHT3XDSensor, config[CONF_MAKE_ID], rhs)

    if CONF_ACCURACY in config:
        add(sht3xd.Psht3xd.set_accuracy(SHT_ACCURACIES[config[CONF_ACCURACY]]))

    sensor.setup_sensor(sht3xd.Psht3xd.Pget_temperature_sensor(), sht3xd.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(sht3xd.Psht3xd.Pget_humidity_sensor(), sht3xd.Pmqtt_humidity,
                        config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_SHT3XD'
