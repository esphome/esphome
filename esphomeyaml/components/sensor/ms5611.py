import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_MAKE_ID, CONF_NAME, CONF_PRESSURE, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

MakeMS5611Sensor = Application.MakeMS5611Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMS5611Sensor),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Required(CONF_PRESSURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    rhs = App.make_ms5611_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)

    if CONF_ADDRESS in config:
        add(make.Pms5611.set_address(config[CONF_ADDRESS]))

    sensor.setup_sensor(make.Pms5611.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(make.Pms5611.Pget_pressure_sensor(), make.Pmqtt_pressure,
                        config[CONF_PRESSURE])


BUILD_FLAGS = '-DUSE_MS5611'
