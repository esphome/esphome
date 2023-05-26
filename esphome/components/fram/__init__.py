import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_TYPE, CONF_SIZE

CODEOWNERS = ["@sharkydog"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

fram_ns = cg.esphome_ns.namespace("fram")
FramComponent = fram_ns.class_("Fram", cg.Component, i2c.I2CDevice)
Fram9Component = fram_ns.class_("Fram9", FramComponent)
Fram11Component = fram_ns.class_("Fram11", FramComponent)
Fram17Component = fram_ns.class_("Fram17", FramComponent)


def validate_bytes_1024(value):
    value = cv.string(value).lower()
    match = re.match(r"^([0-9]+)\s*(\w*)$", value)

    if match is None:
        raise cv.Invalid(f"Expected number of bytes with unit, got {value}")

    SUFF_LIST = ["B", "KB", "KiB", "MB", "MiB"]
    SUFF = {
        "": 1,
        "b": 1,
        "kb": 1000,
        "kib": 1024,
        "mb": pow(1000, 2),
        "mib": pow(1024, 2),
    }

    if match.group(2) not in SUFF:
        raise cv.Invalid(f"Invalid metric suffix {match.group(2)}, valid: {SUFF_LIST}")

    return int(int(match.group(1)) * SUFF[match.group(2)])


FRAM_SCHEMA = (
    cv.Schema({cv.Optional(CONF_SIZE): validate_bytes_1024})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x50))
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "FRAM": FRAM_SCHEMA.extend({cv.GenerateID(): cv.declare_id(FramComponent)}),
        "FRAM9": FRAM_SCHEMA.extend({cv.GenerateID(): cv.declare_id(Fram9Component)}),
        "FRAM11": FRAM_SCHEMA.extend({cv.GenerateID(): cv.declare_id(Fram11Component)}),
        "FRAM17": FRAM_SCHEMA.extend({cv.GenerateID(): cv.declare_id(Fram17Component)}),
    },
    key=CONF_TYPE,
    default_type="FRAM",
    upper=True,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_SIZE in config:
        cg.add(var.set_size_bytes(config[CONF_SIZE]))
