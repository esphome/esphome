import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_PRESSURE, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import HexIntLiteral, Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

BMP085Component = sensor.sensor_ns.class_('BMP085Component', PollingComponent, i2c.I2CDevice)
BMP085TemperatureSensor = sensor.sensor_ns.class_('BMP085TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BMP085PressureSensor = sensor.sensor_ns.class_('BMP085PressureSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BMP085Component),
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
    bmp = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(bmp.set_address(HexIntLiteral(config[CONF_ADDRESS])))

    sensor.setup_sensor(bmp.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(bmp.Pget_pressure_sensor(), config[CONF_PRESSURE])
    setup_component(bmp, config)


BUILD_FLAGS = '-DUSE_BMP085_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE])]
