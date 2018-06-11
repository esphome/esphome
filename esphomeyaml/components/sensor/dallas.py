import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.dallas import DallasComponent
from esphomeyaml.const import CONF_ADDRESS, CONF_DALLAS_ID, CONF_INDEX, CONF_NAME, \
    CONF_RESOLUTION
from esphomeyaml.helpers import HexIntLiteral, get_variable

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    vol.Exclusive(CONF_ADDRESS, 'dallas'): cv.hex_int,
    vol.Exclusive(CONF_INDEX, 'dallas'): cv.positive_int,
    cv.GenerateID(CONF_DALLAS_ID): cv.use_variable_id(DallasComponent),
    vol.Optional(CONF_RESOLUTION): vol.All(vol.Coerce(int), vol.Range(min=9, max=12)),
}), cv.has_at_least_one_key(CONF_ADDRESS, CONF_INDEX))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_DALLAS_ID]):
        yield
    if CONF_ADDRESS in config:
        address = HexIntLiteral(config[CONF_ADDRESS])
        rhs = hub.Pget_sensor_by_address(config[CONF_NAME], address, config.get(CONF_RESOLUTION))
    else:
        rhs = hub.Pget_sensor_by_index(config[CONF_NAME], config[CONF_INDEX],
                                       config.get(CONF_RESOLUTION))
    sensor.register_sensor(rhs, config)


BUILD_FLAGS = '-DUSE_DALLAS_SENSOR'
