import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_MAKE_ID, CONF_NAME, CONF_PRESSURE, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, HexIntLiteral, add, variable, setup_component

DEPENDENCIES = ['i2c']

MakeBMP085Sensor = Application.struct('MakeBMP085Sensor')
BMP085TemperatureSensor = sensor.sensor_ns.class_('BMP085TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BMP085PressureSensor = sensor.sensor_ns.class_('BMP085PressureSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeBMP085Sensor),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP085TemperatureSensor),
    })),
    vol.Required(CONF_PRESSURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP085PressureSensor),
    })),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_bmp085_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    bmp = variable(config[CONF_MAKE_ID], rhs)
    if CONF_ADDRESS in config:
        add(bmp.Pbmp.set_address(HexIntLiteral(config[CONF_ADDRESS])))

    sensor.setup_sensor(bmp.Pbmp.Pget_temperature_sensor(), bmp.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(bmp.Pbmp.Pget_pressure_sensor(), bmp.Pmqtt_pressure,
                        config[CONF_PRESSURE])
    setup_component(bmp.Pbmp, config)


BUILD_FLAGS = '-DUSE_BMP085_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE])]
