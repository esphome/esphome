import voluptuous as vol

from esphome.components import binary_sensor
from esphome.components.pn532 import PN532Component
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_UID
from esphome.core import HexInt
from esphome.cpp_generator import get_variable

DEPENDENCIES = ['pn532']

CONF_PN532_ID = 'pn532_id'


def validate_uid(value):
    value = cv.string_strict(value)
    for x in value.split('-'):
        if len(x) != 2:
            raise vol.Invalid("Each part (separated by '-') of the UID must be two characters "
                              "long.")
        try:
            x = int(x, 16)
        except ValueError:
            raise vol.Invalid("Valid characters for parts of a UID are 0123456789ABCDEF.")
        if x < 0 or x > 255:
            raise vol.Invalid("Valid values for UID parts (separated by '-') are 00 to FF")
    return value


PN532BinarySensor = binary_sensor.binary_sensor_ns.class_('PN532BinarySensor',
                                                          binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PN532BinarySensor),
    vol.Required(CONF_UID): validate_uid,
    cv.GenerateID(CONF_PN532_ID): cv.use_variable_id(PN532Component)
}))


def to_code(config):
    for hub in get_variable(config[CONF_PN532_ID]):
        yield
    addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split('-')]
    rhs = hub.make_tag(config[CONF_NAME], addr)
    binary_sensor.register_binary_sensor(rhs, config)


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
