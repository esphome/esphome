import voluptuous as vol

from esphomeyaml.components import sensor
from esphomeyaml.components.time import sntp
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_TIME_ID
from esphomeyaml.helpers import App, Application, get_variable, variable, setup_component

DEPENDENCIES = ['time']

CONF_POWER_ID = 'power_id'
MakeTotalDailyEnergySensor = Application.MakeTotalDailyEnergySensor
TotalDailyEnergy = sensor.sensor_ns.TotalDailyEnergy

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TotalDailyEnergy),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTotalDailyEnergySensor),
    cv.GenerateID(CONF_TIME_ID): cv.use_variable_id(sntp.SNTPComponent),
    vol.Required(CONF_POWER_ID): cv.use_variable_id(None),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for time in get_variable(config[CONF_TIME_ID]):
        yield
    for sens in get_variable(config[CONF_POWER_ID]):
        yield
    rhs = App.make_total_daily_energy_sensor(config[CONF_NAME], time, sens)
    make = variable(config[CONF_MAKE_ID], rhs)
    total_energy = make.Ptotal_energy

    sensor.setup_sensor(total_energy, make.Pmqtt, config)
    setup_component(total_energy, config)


BUILD_FLAGS = '-DUSE_TOTAL_DAILY_ENERGY_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
