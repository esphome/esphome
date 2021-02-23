from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_ID, CONF_RED, CONF_WHITE

ColorStruct = cg.esphome_ns.struct("Color")

MULTI_CONF = True


def percentage_or_uint8_t(value):

    if isinstance(value, str):
        try:
            if value.endswith("%"):
                value = int(255 * float(value[:-1].rstrip()) / 100.0)
            else:
                value = int(value)
        except ValueError:
            raise cv.Invalid("invalid number")
    if value > 255:
        msg = "Percentage must not be higher than 100% or value greater than 255."
        raise cv.Invalid(msg)
    if value < 0:
        raise cv.Invalid("Value must not be less than 0.")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(ColorStruct),
        cv.Optional(CONF_RED, default=0): percentage_or_uint8_t,
        cv.Optional(CONF_GREEN, default=0): percentage_or_uint8_t,
        cv.Optional(CONF_BLUE, default=0): percentage_or_uint8_t,
        cv.Optional(CONF_WHITE, default=0): percentage_or_uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    cg.variable(
        config[CONF_ID],
        cg.StructInitializer(
            ColorStruct,
            ("r", config[CONF_RED]),
            ("g", config[CONF_GREEN]),
            ("b", config[CONF_BLUE]),
            ("w", config[CONF_WHITE]),
        ),
    )
