import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor, rdm6300
from esphomeyaml.const import CONF_NAME, CONF_UID
from esphomeyaml.helpers import get_variable

DEPENDENCIES = ['rdm6300']

CONF_RDM6300_ID = 'rdm6300_id'

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_UID): cv.uint32_t,
    cv.GenerateID(CONF_RDM6300_ID): cv.use_variable_id(rdm6300.RDM6300Component)
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_RDM6300_ID]):
        yield
    rhs = hub.make_card(config[CONF_NAME], config[CONF_UID])
    binary_sensor.register_binary_sensor(rhs, config)
