import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.pn532 import PN532Component
from esphomeyaml.const import CONF_NAME, CONF_UID
from esphomeyaml.core import HexInt
from esphomeyaml.helpers import ArrayInitializer, get_variable

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


PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_UID): validate_uid,
    cv.GenerateID(CONF_PN532_ID): cv.use_variable_id(PN532Component)
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_PN532_ID]):
        yield
    addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split('-')]
    rhs = hub.make_tag(config[CONF_NAME], ArrayInitializer(*addr, multiline=False))
    binary_sensor.register_binary_sensor(rhs, config)
