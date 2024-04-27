import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_UID
from esphome.core import HexInt
from .. import nfc_ns, Nfcc, NfcTagListener

DEPENDENCIES = ["nfc"]

CONF_NDEF_CONTAINS = "ndef_contains"
CONF_NFCC_ID = "nfcc_id"
CONF_TAG_ID = "tag_id"

NfcTagBinarySensor = nfc_ns.class_(
    "NfcTagBinarySensor",
    binary_sensor.BinarySensor,
    cg.Component,
    NfcTagListener,
    cg.Parented.template(Nfcc),
)


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


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(NfcTagBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_NFCC_ID): cv.use_id(Nfcc),
            cv.Optional(CONF_NDEF_CONTAINS): cv.string,
            cv.Optional(CONF_TAG_ID): cv.string,
            cv.Optional(CONF_UID): validate_uid,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_NDEF_CONTAINS, CONF_TAG_ID, CONF_UID),
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_NFCC_ID])

    hub = await cg.get_variable(config[CONF_NFCC_ID])
    cg.add(hub.register_listener(var))
    if CONF_NDEF_CONTAINS in config:
        cg.add(var.set_ndef_match_string(config[CONF_NDEF_CONTAINS]))
    if CONF_TAG_ID in config:
        cg.add(var.set_tag_name(config[CONF_TAG_ID]))
    elif CONF_UID in config:
        addr = [HexInt(int(x, 16)) for x in config[CONF_UID].split("-")]
        cg.add(var.set_uid(addr))
