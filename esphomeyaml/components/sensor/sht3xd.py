import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ACCURACY, CONF_ADDRESS, CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, variable

DEPENDENCIES = ['i2c']

MakeSHT3XDSensor = Application.MakeSHT3XDSensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeSHT3XDSensor),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_ADDRESS, default=0x44): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,

    vol.Optional(CONF_ACCURACY): cv.invalid("The accuracy option has been removed and now "
                                            "defaults to HIGH."),
})


def to_code(config):
    rhs = App.make_sht3xd_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    sht3xd = variable(config[CONF_MAKE_ID], rhs)

    sensor.setup_sensor(sht3xd.Psht3xd.Pget_temperature_sensor(), sht3xd.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(sht3xd.Psht3xd.Pget_humidity_sensor(), sht3xd.Pmqtt_humidity,
                        config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_SHT3XD'
