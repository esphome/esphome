import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_UID
from esphome.core import HexInt
from . import pn7160_ns, PN7160, CONF_PN7160_ID

DEPENDENCIES = ["pn7160"]

CONF_NDEF_CONTAINS = "ndef_contains"
CONF_TAG_ID = "tag_id"


def validate_uid(value):
    value = cv.string_strict(value)
    for x in value.split("-"):
        if len(x) != 2:
            raise cv.Invalid(
                "Each part (separated by '-') of the UID must be two characters "
                "long."
            )
        try:
            x = int(x, 16)
        except ValueError as err:
            raise cv.Invalid(
                "Valid characters for parts of a UID are 0123456789ABCDEF."
            ) from err
        if x < 0 or x > 255:
            raise cv.Invalid(
                "Valid values for UID parts (separated by '-') are 00 to FF"
            )
    return value


PN532BinarySensor = pn7160_ns.class_("PN7160BinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(PN532BinarySensor).extend(
        {
            cv.GenerateID(CONF_PN7160_ID): cv.use_id(PN7160),
            cv.Optional(CONF_NDEF_CONTAINS): cv.string,
            cv.Optional(CONF_TAG_ID): cv.string,
            cv.Optional(CONF_UID): validate_uid,
        }
    ),
    cv.has_exactly_one_key(CONF_NDEF_CONTAINS, CONF_TAG_ID, CONF_UID),
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    hub = await cg.get_variable(config[CONF_PN7160_ID])
    cg.add(hub.register_tag(var))
    if CONF_NDEF_CONTAINS in config:
        cg.add(var.set_ndef_match_string(config[CONF_NDEF_CONTAINS]))
    if CONF_TAG_ID in config:
        cg.add(var.set_tag_name(config[CONF_TAG_ID]))
    elif CONF_UID in config:
        addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split("-")]
        cg.add(var.set_uid(addr))
