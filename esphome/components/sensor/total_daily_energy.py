import voluptuous as vol

from esphome.components import sensor, time
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_TIME_ID
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['time']

CONF_POWER_ID = 'power_id'
TotalDailyEnergy = sensor.sensor_ns.class_('TotalDailyEnergy', sensor.Sensor, Component)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TotalDailyEnergy),
    cv.GenerateID(CONF_TIME_ID): cv.use_variable_id(time.RealTimeClockComponent),
    vol.Required(CONF_POWER_ID): cv.use_variable_id(sensor.Sensor),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for time_ in get_variable(config[CONF_TIME_ID]):
        yield
    for sens in get_variable(config[CONF_POWER_ID]):
        yield
    rhs = App.make_total_daily_energy_sensor(config[CONF_NAME], time_, sens)
    total_energy = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(total_energy, config)
    setup_component(total_energy, config)


BUILD_FLAGS = '-DUSE_TOTAL_DAILY_ENERGY_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
