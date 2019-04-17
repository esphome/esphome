import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_NAME, CONF_UID, CONF_ID
from esphome.core import HexInt
from . import pn532_ns, PN532

DEPENDENCIES = ['pn532']

CONF_PN532_ID = 'pn532_id'


def validate_uid(value):
    value = cv.string_strict(value)
    for x in value.split('-'):
        if len(x) != 2:
            raise cv.Invalid("Each part (separated by '-') of the UID must be two characters "
                             "long.")
        try:
            x = int(x, 16)
        except ValueError:
            raise cv.Invalid("Valid characters for parts of a UID are 0123456789ABCDEF.")
        if x < 0 or x > 255:
            raise cv.Invalid("Valid values for UID parts (separated by '-') are 00 to FF")
    return value


PN532BinarySensor = pn532_ns.class_('PN532BinarySensor', binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PN532BinarySensor),
    cv.GenerateID(CONF_PN532_ID): cv.use_variable_id(PN532),
    cv.Required(CONF_UID): validate_uid,
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_PN532_ID])
    addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split('-')]
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], addr)
    cg.add(hub.register_tag(var))
    yield binary_sensor.register_binary_sensor(var, config)
