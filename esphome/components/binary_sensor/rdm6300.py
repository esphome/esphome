import voluptuous as vol

from esphome.components import binary_sensor, rdm6300
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_UID
from esphome.cpp_generator import get_variable

DEPENDENCIES = ['rdm6300']

CONF_RDM6300_ID = 'rdm6300_id'

RDM6300BinarySensor = binary_sensor.binary_sensor_ns.class_('RDM6300BinarySensor',
                                                            binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RDM6300BinarySensor),
    vol.Required(CONF_UID): cv.uint32_t,
    cv.GenerateID(CONF_RDM6300_ID): cv.use_variable_id(rdm6300.RDM6300Component)
}))


def to_code(config):
    for hub in get_variable(config[CONF_RDM6300_ID]):
        yield
    rhs = hub.make_card(config[CONF_NAME], config[CONF_UID])
    binary_sensor.register_binary_sensor(rhs, config)


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
