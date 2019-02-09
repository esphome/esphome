import voluptuous as vol

from esphome.components import binary_sensor, sensor
from esphome.components.apds9960 import APDS9960, CONF_APDS9960_ID
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_NAME
from esphome.cpp_generator import get_variable

DEPENDENCIES = ['apds9960']
APDS9960GestureDirectionBinarySensor = sensor.sensor_ns.class_(
    'APDS9960GestureDirectionBinarySensor', binary_sensor.BinarySensor)

DIRECTIONS = {
    'UP': 'make_up_direction',
    'DOWN': 'make_down_direction',
    'LEFT': 'make_left_direction',
    'RIGHT': 'make_right_direction',
}

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(APDS9960GestureDirectionBinarySensor),
    vol.Required(CONF_DIRECTION): cv.one_of(*DIRECTIONS, upper=True),
    cv.GenerateID(CONF_APDS9960_ID): cv.use_variable_id(APDS9960)
}))


def to_code(config):
    for hub in get_variable(config[CONF_APDS9960_ID]):
        yield
    func = getattr(hub, DIRECTIONS[config[CONF_DIRECTION]])
    rhs = func(config[CONF_NAME])
    binary_sensor.register_binary_sensor(rhs, config)


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
