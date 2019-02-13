import voluptuous as vol

from esphome.components import sensor
from esphome.components.apds9960 import APDS9960, CONF_APDS9960_ID
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_TYPE
from esphome.cpp_generator import get_variable

DEPENDENCIES = ['apds9960']

TYPES = {
    'CLEAR': 'make_clear_channel',
    'RED': 'make_red_channel',
    'GREEN': 'make_green_channel',
    'BLUE': 'make_blue_channel',
    'PROXIMITY': 'make_proximity',
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(sensor.Sensor),
    vol.Required(CONF_TYPE): cv.one_of(*TYPES, upper=True),
    cv.GenerateID(CONF_APDS9960_ID): cv.use_variable_id(APDS9960)
}))


def to_code(config):
    for hub in get_variable(config[CONF_APDS9960_ID]):
        yield
    func = getattr(hub, TYPES[config[CONF_TYPE]])
    rhs = func(config[CONF_NAME])
    sensor.register_sensor(rhs, config)


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
