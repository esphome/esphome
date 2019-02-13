import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_PRESSURE, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

MS5611Component = sensor.sensor_ns.class_('MS5611Component', PollingComponent, i2c.I2CDevice)
MS5611TemperatureSensor = sensor.sensor_ns.class_('MS5611TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
MS5611PressureSensor = sensor.sensor_ns.class_('MS5611PressureSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MS5611Component),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MS5611TemperatureSensor),
    })),
    vol.Required(CONF_PRESSURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MS5611PressureSensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_ms5611_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    ms5611 = Pvariable(config[CONF_ID], rhs)

    if CONF_ADDRESS in config:
        add(ms5611.set_address(config[CONF_ADDRESS]))

    sensor.setup_sensor(ms5611.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(ms5611.Pget_pressure_sensor(), config[CONF_PRESSURE])
    setup_component(ms5611, config)


BUILD_FLAGS = '-DUSE_MS5611'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE])]
