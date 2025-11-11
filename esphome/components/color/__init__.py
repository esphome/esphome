from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_ID, CONF_RED, CONF_WHITE

ColorStruct = cg.esphome_ns.struct("Color")

INSTANCE_TYPE = ColorStruct

MULTI_CONF = True

CONF_RED_INT = "red_int"
CONF_GREEN_INT = "green_int"
CONF_BLUE_INT = "blue_int"
CONF_WHITE_INT = "white_int"
CONF_HEX = "hex"


def hex_color(value):
    if isinstance(value, int):
        value = str(value)
    if not isinstance(value, str):
        raise cv.Invalid("Invalid value for hex color")
    if len(value) != 6:
        raise cv.Invalid("Hex color must have six digits")
    try:
        return int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16)
    except ValueError as exc:
        raise cv.Invalid("Color must be hexadecimal") from exc


components = {
    CONF_RED,
    CONF_RED_INT,
    CONF_GREEN,
    CONF_GREEN_INT,
    CONF_BLUE,
    CONF_BLUE_INT,
    CONF_WHITE,
    CONF_WHITE_INT,
}


def validate_color(config):
    has_components = set(config) & components
    has_hex = CONF_HEX in config
    if has_hex and has_components:
        raise cv.Invalid("Hex color value may not be combined with component values")
    if not has_hex and not has_components:
        raise cv.Invalid("Must provide at least one color option")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(ColorStruct),
            cv.Exclusive(CONF_RED, "red"): cv.percentage,
            cv.Exclusive(CONF_RED_INT, "red"): cv.uint8_t,
            cv.Exclusive(CONF_GREEN, "green"): cv.percentage,
            cv.Exclusive(CONF_GREEN_INT, "green"): cv.uint8_t,
            cv.Exclusive(CONF_BLUE, "blue"): cv.percentage,
            cv.Exclusive(CONF_BLUE_INT, "blue"): cv.uint8_t,
            cv.Exclusive(CONF_WHITE, "white"): cv.percentage,
            cv.Exclusive(CONF_WHITE_INT, "white"): cv.uint8_t,
            cv.Optional(CONF_HEX): hex_color,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_color,
)


def from_rgbw(config):
    r = 0
    if CONF_RED in config:
        r = int(config[CONF_RED] * 255)
    elif CONF_RED_INT in config:
        r = config[CONF_RED_INT]

    g = 0
    if CONF_GREEN in config:
        g = int(config[CONF_GREEN] * 255)
    elif CONF_GREEN_INT in config:
        g = config[CONF_GREEN_INT]

    b = 0
    if CONF_BLUE in config:
        b = int(config[CONF_BLUE] * 255)
    elif CONF_BLUE_INT in config:
        b = config[CONF_BLUE_INT]

    w = 0
    if CONF_WHITE in config:
        w = int(config[CONF_WHITE] * 255)
    elif CONF_WHITE_INT in config:
        w = config[CONF_WHITE_INT]

    return (r, g, b, w)


async def to_code(config):
    if CONF_HEX in config:
        r, g, b = config[CONF_HEX]
        w = 0
    else:
        r, g, b, w = from_rgbw(config)

    cg.new_variable(
        config[CONF_ID],
        cg.ArrayInitializer(r, g, b, w),
    )
